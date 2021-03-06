/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "messagequeue.h"
#include "control.h"

#include <QTcpSocket>
#include <QDateTime>
#include <QTimer>
#include <cstring>

#ifndef NDEBUG
#include <QThread>
#endif

namespace protocol {

// Reserve enough buffer space for one complete message
static const int MAX_BUF_LEN = 1024*64 + protocol::Message::HEADER_LEN;

MessageQueue::MessageQueue(QTcpSocket *socket, QObject *parent)
	: QObject(parent), m_socket(socket),
	  m_pingTimer(nullptr),
	  m_lastRecvTime(0),
	  m_idleTimeout(0), m_pingSent(0), m_closeWhenReady(false),
	  m_ignoreIncoming(false),
	  m_decodeOpaque(false)
{
	connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));
	connect(socket, SIGNAL(bytesWritten(qint64)), this, SLOT(dataWritten(qint64)));

	if(socket->inherits("QSslSocket")) {
		connect(socket, SIGNAL(encrypted()), this, SLOT(sslEncrypted()));
	}

	m_recvbuffer = new char[MAX_BUF_LEN];
	m_sendbuffer = new char[MAX_BUF_LEN];
	m_recvcount = 0;
	m_sentcount = 0;
	m_sendbuflen = 0;

	m_idleTimer = new QTimer(this);
	connect(m_idleTimer, &QTimer::timeout, this, &MessageQueue::checkIdleTimeout);
	m_idleTimer->setInterval(1000);
	m_idleTimer->setSingleShot(false);

#ifndef NDEBUG
	m_randomlag = 0;
#endif
}

void MessageQueue::sslEncrypted()
{
	disconnect(m_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(dataWritten(qint64)));
	connect(m_socket, SIGNAL(encryptedBytesWritten(qint64)), this, SLOT(dataWritten(qint64)));
}

void MessageQueue::checkIdleTimeout()
{
	if(m_idleTimeout>0 && m_socket->state() == QTcpSocket::ConnectedState && idleTime() > m_idleTimeout) {
		qWarning("MessageQueue timeout");
		m_socket->abort();
	}
}

void MessageQueue::setIdleTimeout(qint64 timeout)
{
	m_idleTimeout = timeout;
	m_lastRecvTime = QDateTime::currentMSecsSinceEpoch();
	if(timeout>0)
		m_idleTimer->start(1000);
	else
		m_idleTimer->stop();
}

void MessageQueue::setPingInterval(int msecs)
{
	if(!m_pingTimer) {
		m_pingTimer = new QTimer(this);
		m_pingTimer->setSingleShot(false);
		connect(m_pingTimer, SIGNAL(timeout()), this, SLOT(sendPing()));
	}
	m_pingTimer->setInterval(msecs);
	m_pingTimer->start(msecs);
}

MessageQueue::~MessageQueue()
{
	delete [] m_recvbuffer;
	delete [] m_sendbuffer;
}

bool MessageQueue::isPending() const
{
	return !m_recvqueue.isEmpty();
}

MessagePtr MessageQueue::getPending()
{
	return m_recvqueue.dequeue();
}

void MessageQueue::send(MessagePtr packet)
{
	if(!m_closeWhenReady) {
		m_sendqueue.enqueue(packet);
		if(m_sendbuflen==0)
			writeData();
	}
}

void MessageQueue::sendNow(MessagePtr msg)
{
	if(!m_closeWhenReady) {
		m_sendqueue.prepend(msg);
		if(m_sendbuflen==0)
			writeData();
	}
}

void MessageQueue::sendDisconnect(int reason, const QString &message)
{
	send(MessagePtr(new protocol::Disconnect(0, protocol::Disconnect::Reason(reason), message)));
	m_ignoreIncoming = true;
	m_recvcount = 0;
}

void MessageQueue::sendPing()
{
	if(m_pingSent==0) {
		m_pingSent = QDateTime::currentMSecsSinceEpoch();
	} else {
		// This shouldn't happen, but we'll resend a ping anyway just to be safe.
		qWarning("sendPing(): reply to previous ping not yet received!");
	}

	sendNow(MessagePtr(new Ping(0, false)));
}

int MessageQueue::uploadQueueBytes() const
{
	int total = m_socket->bytesToWrite() + m_sendbuflen - m_sentcount;
	for(const MessagePtr msg : m_sendqueue)
		total += msg->length();
	return total;
}

qint64 MessageQueue::idleTime() const
{
	return QDateTime::currentMSecsSinceEpoch() - m_lastRecvTime;
}

void MessageQueue::readData() {
	bool gotmessage = false;
	int read, totalread=0;
	do {
		// Read as much as fits in to the deserialization buffer
		read = m_socket->read(m_recvbuffer+m_recvcount, MAX_BUF_LEN-m_recvcount);
		if(read<0) {
			emit socketError(m_socket->errorString());
			return;
		}

		if(m_ignoreIncoming) {
			// Ignore incoming data mode is used when we're shutting down the connection
			// but want to clear the upload queue
			if(read>0)
				continue;
			else
				return;
		}

		m_recvcount += read;

		// Extract all complete messages
		int len;
		while(m_recvcount >= Message::HEADER_LEN && m_recvcount >= (len=Message::sniffLength(m_recvbuffer))) {
			// Whole message received!
			Message *message = Message::deserialize((const uchar*)m_recvbuffer, m_recvcount, m_decodeOpaque);
			if(!message) {
				emit badData(len, m_recvbuffer[2]);

			} else {
				MessagePtr msg(message);

				if(msg->type() == MSG_STREAMPOS) {
					// Special handling for Stream Position message
					emit expectingBytes(msg.cast<StreamPos>().bytes() + totalread);

				} else if(msg->type() == MSG_PING) {
					// Special handling for Ping messages
					bool isPong = msg.cast<Ping>().isPong();

					if(isPong) {
						if(m_pingSent==0) {
							qWarning("Received Pong, but no Ping was sent!");

						} else {
							qint64 roundtrip = QDateTime::currentMSecsSinceEpoch() - m_pingSent;
							m_pingSent = 0;
							emit pingPong(roundtrip);
						}
					} else {
						sendNow(MessagePtr(new Ping(0, true)));
					}

				} else {
					m_recvqueue.enqueue(msg);
					gotmessage = true;
				}
			}

			if(len < m_recvcount) {
				memmove(m_recvbuffer, m_recvbuffer+len, m_recvcount-len);
			}
			m_recvcount -= len;
		}

		// All messages extracted from buffer (if there were any):
		// see if there are more bytes in the socket buffer
		totalread += read;
	} while(read>0);

	if(totalread) {
		m_lastRecvTime = QDateTime::currentMSecsSinceEpoch();
		emit bytesReceived(totalread);
	}

	if(gotmessage)
		emit messageAvailable();
}

void MessageQueue::dataWritten(qint64 bytes)
{
	emit bytesSent(bytes);

	// Write more once the buffer is empty
	if(m_socket->bytesToWrite()==0) {
		if(m_sendbuflen==0 && m_sendqueue.isEmpty())
			emit allSent();
		else
			writeData();
	}
}

void MessageQueue::writeData() {
	if(m_sendbuflen==0 && !m_sendqueue.isEmpty()) {
		MessagePtr msg = m_sendqueue.dequeue();
		m_sendbuflen = msg->serialize(m_sendbuffer);

		if(msg->type() == protocol::MSG_DISCONNECT) {
			// Automatically disconnect after Disconnect notification is sent
			m_closeWhenReady = true;
			m_sendqueue.clear();
		}
	}

	if(m_sentcount < m_sendbuflen) {
#ifndef NDEBUG
		// Debugging tool: simulate bad network connections by sleeping at odd times
		if(m_randomlag>0) {
			QThread::msleep(qrand() % m_randomlag);
		}
#endif

		int sent = m_socket->write(m_sendbuffer+m_sentcount, m_sendbuflen-m_sentcount);
		if(sent<0) {
			// Error
			emit socketError(m_socket->errorString());
			return;
		}
		m_sentcount += sent;
		if(m_sentcount == m_sendbuflen) {
			m_sendbuflen=0;
			m_sentcount=0;
			if(m_closeWhenReady) {
				m_socket->disconnectFromHost();

			} else {
				writeData();
			}
		}
	}
}

}

