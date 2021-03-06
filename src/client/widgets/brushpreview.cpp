/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#include <QDebug>

#include <QPaintEvent>
#include <QPainter>
#include <QEvent>
#include <QMenu>

#include "core/point.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "core/shapes.h"
#include "core/floodfill.h"
#include "brushpreview.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), _preview(0), _sizepressure(false),
	_opacitypressure(false), _hardnesspressure(false), _smudgepressure(false),
	m_color(Qt::black), m_bg(Qt::white),
	_shape(Stroke), _fillTolerance(0), _fillExpansion(0), _underFill(false), _tranparentbg(false)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32,32);

	_ctxmenu = new QMenu(this);

	_ctxmenu->addAction(tr("Change Foreground Color"), this, SIGNAL(requestColorChange()));
}

BrushPreview::~BrushPreview() {
	delete _preview;
}

void BrushPreview::setPreviewShape(PreviewShape shape)
{
	_shape = shape;
	_needupdate = true;
	update();
}

void BrushPreview::setTransparentBackground(bool transparent)
{
	_tranparentbg = transparent;
	_needupdate = true;
	update();
}

void BrushPreview::setColor(const QColor& color)
{
	m_color = color;
	_brush.setColor(color);
	_needupdate = true;

	// Decide background color
	const qreal lum = color.redF() * 0.216 + color.greenF() * 0.7152 + color.redF() * 0.0722;

	if(lum < 0.8) {
		m_bg = Qt::white;
	} else {
		m_bg = QColor(32, 32, 32);
	}

	update();
}

void BrushPreview::setFloodFillTolerance(int tolerance)
{
	_fillTolerance = tolerance;
	_needupdate = true;
	update();
}

void BrushPreview::setFloodFillExpansion(int expansion)
{
	_fillExpansion = expansion;
	_needupdate = true;
	update();
}

void BrushPreview::setUnderFill(bool underfill)
{
	_underFill = underfill;
	_needupdate = true;
	update();
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	_needupdate = true;
}

void BrushPreview::changeEvent(QEvent *event)
{
	Q_UNUSED(event);
	_needupdate = true;
	update();
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	if(_needupdate)
		updatePreview();

	if((_previewCache.isNull() || _previewCache.size() != _preview->size()) && _preview->size().isValid())
		_previewCache = QPixmap(_preview->size());

	_preview->paintChangedTiles(event->rect(), &_previewCache);

	QPainter painter(this);
	painter.drawPixmap(event->rect(), _previewCache, event->rect());
}

void BrushPreview::updatePreview()
{
	if(_preview==0) {
		_preview = new paintcore::LayerStack;
		QSize size = contentsRect().size();
		_preview->resize(0, size.width(), size.height(), 0);
		_preview->createLayer(0, 0, QColor(0,0,0), false, false, QString());
	} else if(_preview->width() != contentsRect().width() || _preview->height() != contentsRect().height()) {
		_preview->resize(0, contentsRect().width() - _preview->width(), contentsRect().height() - _preview->height(), 0);
	}

	QRectF previewRect(
		_preview->width()/8,
		_preview->height()/4,
		_preview->width()-_preview->width()/4,
		_preview->height()-_preview->height()/2
	);
	paintcore::PointVector pointvector;

	switch(_shape) {
	case Stroke: pointvector = paintcore::shapes::sampleStroke(previewRect); break;
	case Line:
		pointvector
			<< paintcore::Point(previewRect.left(), previewRect.top(), 1.0)
			<< paintcore::Point(previewRect.right(), previewRect.bottom(), 1.0);
		break;
	case Rectangle: pointvector = paintcore::shapes::rectangle(previewRect); break;
	case Ellipse: pointvector = paintcore::shapes::ellipse(previewRect); break;
	case FloodFill: pointvector = paintcore::shapes::sampleBlob(previewRect); break;
	}

	QColor bgcolor = m_bg;

	paintcore::Brush brush = _brush;
	// Special handling for some blending modes
	// TODO this could be implemented in some less ad-hoc way
	if(brush.blendingMode() == 11) {
		// "behind" mode needs a transparent layer for anything to show up
		brush.setBlendingMode(paintcore::BlendMode::MODE_NORMAL);

	} else if(brush.blendingMode() == 12) {
		// Color-erase mode: use fg color as background
		bgcolor = m_color;
	}

	if(_shape == FloodFill) {
		brush.setColor(bgcolor);
	}

	paintcore::Layer *layer = _preview->getLayerByIndex(0);
	layer->fillRect(QRect(0, 0, layer->width(), layer->height()), isTransparentBackground() ? QColor(Qt::transparent) : bgcolor, paintcore::BlendMode::MODE_REPLACE);

	paintcore::StrokeState ss(brush);
	for(int i=1;i<pointvector.size();++i)
		layer->drawLine(0, brush, pointvector[i-1], pointvector[i], ss);

	layer->mergeSublayer(0);

	if(_shape == FloodFill) {
		paintcore::FillResult fr = paintcore::floodfill(_preview, previewRect.center().toPoint(), m_color, _fillTolerance, 0, false);
		if(_fillExpansion>0)
			fr = paintcore::expandFill(fr, _fillExpansion, m_color);
		layer->putImage(fr.x, fr.y, fr.image, _underFill ? paintcore::BlendMode::MODE_BEHIND : paintcore::BlendMode::MODE_NORMAL);
	}

	_needupdate=false;
}

/**
 * @param brush brush to set
 */
void BrushPreview::setBrush(const paintcore::Brush& brush)
{
	_brush = brush;
	updatePreview();
	update();
}

/**
 * @param size brush size
 */
void BrushPreview::setSize(int size)
{
	_brush.setSize(size);
	if(_sizepressure==false)
		_brush.setSize2(size);
	updatePreview();
	update();
}

/**
 * @param opacity brush opacity
 * @pre 0 <= opacity <= 100
 */
void BrushPreview::setOpacity(int opacity)
{
	const qreal o = opacity/100.0;
	_brush.setOpacity(o);
	if(_opacitypressure==false)
		_brush.setOpacity2(o);
	updatePreview();
	update();
}

/**
 * @param hardness brush hardness
 * @pre 0 <= hardness <= 100
 */
void BrushPreview::setHardness(int hardness)
{
	const qreal h = hardness/100.0;
	_brush.setHardness(h);
	if(_hardnesspressure==false)
		_brush.setHardness2(h);
	updatePreview();
	update();
}

/**
 * @param smudge color smudge pressure
 * @pre 0 <= smudge <= 100
 */
void BrushPreview::setSmudge(int smudge)
{
	const qreal s = smudge / 100.0;
	_brush.setSmudge(s);
	if(_smudgepressure==false)
		_brush.setSmudge2(s);
	updatePreview();
	update();
}

/**
 * @param spacing dab spacing
 * @pre 0 <= spacing <= 100
 */
void BrushPreview::setSpacing(int spacing)
{
	_brush.setSpacing(spacing);
	updatePreview();
	update();
}

void BrushPreview::setSmudgeFrequency(int f)
{
	_brush.setResmudge(f);
	updatePreview();
	update();
}

void BrushPreview::setSizePressure(bool enable)
{
	_sizepressure = enable;
	if(enable)
		_brush.setSize2(1);
	else
		_brush.setSize2(_brush.size1());
	updatePreview();
	update();
}

void BrushPreview::setOpacityPressure(bool enable)
{
	_opacitypressure = enable;
	if(enable)
		_brush.setOpacity2(0);
	else
		_brush.setOpacity2(_brush.opacity1());
	updatePreview();
	update();
}

void BrushPreview::setHardnessPressure(bool enable)
{
	_hardnesspressure = enable;
	if(enable)
		_brush.setHardness2(0);
	else
		_brush.setHardness2(_brush.hardness1());
	updatePreview();
	update();
}

void BrushPreview::setSmudgePressure(bool enable)
{
	_smudgepressure = enable;
	if(enable)
		_brush.setSmudge2(0);
	else
		_brush.setSmudge2(_brush.smudge1());
	updatePreview();
	update();
}

void BrushPreview::setSubpixel(bool enable)
{
	_brush.setSubpixel(enable);
	updatePreview();
	update();
}

void BrushPreview::setBlendingMode(paintcore::BlendMode::Mode mode)
{
	_brush.setBlendingMode(mode);
	updatePreview();
	update();
}

void BrushPreview::setHardEdge(bool hard)
{
	if(hard) {
		_oldhardness1 = _brush.hardness(0);
		_oldhardness2 = _brush.hardness(1);
		_brush.setHardness(1);
		_brush.setHardness2(1);
		_brush.setSubpixel(false);
	} else {
		_brush.setHardness(_oldhardness1);
		_brush.setHardness2(_oldhardness2);
		_brush.setSubpixel(true);
	}
	updatePreview();
	update();
}

void BrushPreview::setIncremental(bool incremental)
{
	_brush.setIncremental(incremental);
	updatePreview();
	update();
}

void BrushPreview::contextMenuEvent(QContextMenuEvent *e)
{
	_ctxmenu->popup(e->globalPos());
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent*)
{
	emit requestColorChange();
}

#ifndef DESIGNER_PLUGIN
}
#endif

