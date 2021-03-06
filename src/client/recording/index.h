/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef REC_INDEX_H
#define REC_INDEX_H

#include <QVector>
#include <QHash>
#include <QString>
#include <QSet>

class QIODevice;

namespace recording {

//! Index format version
static const quint16 INDEX_VERSION = 0x0003;

enum IndexType {
	IDX_NULL,        // null/invalid entry
	IDX_MARKER,      // pause point marker
	IDX_RESIZE,      // canvas resize
	IDX_CREATELAYER, // layer creation
	IDX_DELETELAYER, // layer deletion
	IDX_PUTIMAGE,    // put image/fill area
	IDX_STROKE,      // pen stroke
	IDX_ANNOTATE,    // annotation editing
	IDX_CHAT,        // a chat message
	IDX_PAUSE,       // pause
	IDX_LASER,       // laser pointing
	IDX_UNDO,        // undo
	IDX_REDO,        // redo
	IDX_FILL         // fill area
};

struct IndexEntry {
	IndexEntry() : type(IDX_NULL), context_id(0), offset(0), start(0), end(0), color(0), flags(0) { }
	IndexEntry(IndexType typ, int ctx, qint64 o, int s, int e, quint32 c, const QString &title_) : type(typ), context_id(ctx), offset(o), start(s), end(e), color(c), title(title_), flags(0) { }

	//! Type of the index entry
	IndexType type;

	//! Context ID of the message
	quint8 context_id;

	//! Offset of the first message in the file
	qint64 offset;

	//! First stream index of the action
	quint32 start;

	//! Last stream index of the action
	quint32 end;

	//! Color hint for this index entry (RGB)
	quint32 color;

	//! Title/tooltip text for the index entry
	QString title;

	// Flags used when building and modifying the index
	int flags;

	static const int FLAG_FINISHED = 0x01;
	static const int FLAG_ADDED = 0x02;
};

struct MarkerEntry {
	MarkerEntry() : idxpos(-1), pos(0) { }
	MarkerEntry(int idxpos_, int pos_, const QString &title_) : idxpos(idxpos_), pos(pos_), title(title_) { }

	// position of the marker in the index
	int idxpos;

	// position of the marker in the message stream.
	quint32 pos;

	// marker title
	QString title;
};

struct SnapshotEntry {
	SnapshotEntry() : stream_offset(-1), pos(-1) { }
	SnapshotEntry(qint64 offset, quint32 pos_) : stream_offset(offset), pos(pos_) { }

	//! Offset to the start of the snapshot in the message stream (relative to message stream block start)
	qint64 stream_offset;

	//! Stream index of the snapshot
	quint32 pos;
};

typedef QVector<IndexEntry> IndexVector;
typedef QVector<SnapshotEntry> SnapshotVector;

/**
 * Recording index
 */
class Index {
	friend class IndexBuilder;
	friend class IndexLoader;

public:

	//! Get the number of entries in the index
	int size() const { return m_index.size(); }

	//! Get the name associated with the given context ID
	QString contextName(int context_id) const;

	//! Get the given index entry
	const IndexEntry &entry(int idx) const { return m_index.at(idx); }

	//! Get the index entry vector
	const IndexVector &entries() const { return m_index; }

	//! Get all snapshots
	const SnapshotVector &snapshots() const { return m_snapshots; }

	MarkerEntry nextMarker(unsigned int from) const;
	MarkerEntry prevMarker(unsigned int from) const;

	const QVector<MarkerEntry> &markers() const { return m_markers; }
	IndexVector newMarkers() const { return m_newmarkers; }

	//! Add a new marker. This can be turned in to a real marker using the recording filter
	void addMarker(qint64 offset, quint32 pos, const QString &title);

	//! Silence an index entry (silenced actions can be filtered out)
	void setSilenced(int idx, bool silence);
	const QSet<int> silencedIndices() const { return m_silenced; }
	IndexVector silencedEntries() const;

	bool writeIndex(QIODevice *out) const;
	bool readIndex(QIODevice *in);

private:
	IndexVector m_index;
	SnapshotVector m_snapshots;
	QHash<int, QString> m_ctxnames;
	QVector<MarkerEntry> m_markers;
	IndexVector m_newmarkers;
	QSet<int> m_silenced;
};

//! Hash the recording file
QByteArray hashRecording(const QString &filename);

}

#endif
