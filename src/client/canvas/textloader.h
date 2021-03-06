/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef DP_SESSION_TEXTLOADER_H
#define DP_SESSION_TEXTLOADER_H

#include "canvas/loader.h"
#include "canvas/statetracker.h"
#include "canvas/layerlist.h"
#include "core/layer.h"

#include <QHash>

namespace canvas {

/**
 * @brief A session loader that loads a text based command stream
 */
class TextCommandLoader : public SessionLoader {
public:
	TextCommandLoader(const QString &filename) : _filename(filename) {}

	bool load();

	QList<protocol::MessagePtr> loadInitCommands() { return _messages; }
	QString filename() const { return _filename; }
	QString errorMessage() const { return _error; }

private:

	void handleResize(const QString &args);
	void handleNewLayer(const QString &args);
	void handleCopyLayer(const QString &args);
	void handleLayerAttr(const QString &args);
	void handleRetitleLayer(const QString &args);
	void handleDeleteLayer(const QString &args);
	void handleReorderLayers(const QString &args);

	void handleDrawingContext(const QString &args);
	void handlePenMove(const QString &args);
	void handlePenUp(const QString &args);
	void handleInlineImage(const QString &args);
	void handlePutImage(const QString &args);
	void handleFillRect(const QString &args);

	void handleUndoPoint(const QString &args);
	void handleUndo(const QString &args);

	void handleAddAnnotation(const QString &args);
	void handleReshapeAnnotation(const QString &args);
	void handlEditAnnotation(const QString &args);
	void editAnnotationDone();
	void handleDeleteAnnotation(const QString &args);

	QString _filename;
	QString _error;
	QList<protocol::MessagePtr> _messages;
	QHash<int, ToolContext> _ctx;
	QHash<int, paintcore::LayerInfo> _layer;

	// Annotation edit buffer
	int _edit_a_ctx;
	int _edit_a_id;
	quint32 _edit_a_color;
	QString _edit_a_text;

	// Inline image buffer
	int _inlineImageWidth, _inlineImageHeight;
	QByteArray _inlineImageData;
};

}

#endif
