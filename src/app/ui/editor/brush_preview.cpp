// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/editor/brush_preview.h"

#include "app/app.h"
#include "app/color.h"
#include "app/color_utils.h"
#include "app/document.h"
#include "app/tools/controller.h"
#include "app/tools/ink.h"
#include "app/tools/intertwine.h"
#include "app/tools/point_shape.h"
#include "app/tools/tool_loop.h"
#include "app/ui/context_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/tool_loop_impl.h"
#include "app/ui/main_window.h"
#include "app/ui_context.h"
#include "doc/algo.h"
#include "doc/blend_internals.h"
#include "doc/brush.h"
#include "doc/cel.h"
#include "doc/image_impl.h"
#include "doc/layer.h"
#include "doc/primitives.h"
#include "doc/site.h"

namespace app {

using namespace doc;

std::vector<gfx::Color> BrushPreview::m_savedPixels;
int BrushPreview::m_savedPixelsIterator;

BrushPreview::BrushPreview(Editor* editor)
  : m_editor(editor)
  , m_type(CROSS)
  , m_onScreen(false)
  , m_screenPosition(0, 0)
  , m_editorPosition(0, 0)
{
}

BrushPreview::~BrushPreview()
{
}

// static
Brush* BrushPreview::getCurrentBrush()
{
  return App::instance()->getMainWindow()->getContextBar()->activeBrush().get();
}

// static
color_t BrushPreview::getBrushColor(Sprite* sprite, Layer* layer)
{
  app::Color c = Preferences::instance().colorBar.fgColor();
  ASSERT(sprite != NULL);

  // Avoid using invalid colors
  if (!c.isValid())
    return 0;

  if (layer != NULL)
    return color_utils::color_for_layer(c, layer);
  else
    return color_utils::color_for_image(c, sprite->pixelFormat());
}

// Draws the brush cursor in the specified absolute mouse position
// given in 'pos' param.  Warning: You should clean the cursor before
// to use this routine with other editor.
void BrushPreview::show(const gfx::Point& screenPos)
{
  if (m_onScreen)
    hide();

  app::Document* document = m_editor->document();
  Sprite* sprite = m_editor->sprite();
  Layer* layer = m_editor->layer();
  ASSERT(sprite);

  // Get drawable region
  m_editor->getDrawableRegion(m_clippingRegion, ui::Widget::kCutTopWindows);

  // Get cursor color
  app::Color app_cursor_color = Preferences::instance().editor.cursorColor();
  gfx::Color ui_cursor_color = color_utils::color_for_ui(app_cursor_color);
  m_blackAndWhiteNegative = (app_cursor_color.getType() == app::Color::MaskType);

  // Cursor in the screen (view)
  m_screenPosition = screenPos;

  // Get cursor position in the editor
  gfx::Point spritePos = m_editor->screenToEditor(screenPos);

  // Get the current tool
  tools::Ink* ink = m_editor->getCurrentEditorInk();

  // Setup the cursor type debrushding of several factors (current tool,
  // foreground color, and layer transparency).
  color_t brush_color = getBrushColor(sprite, layer);
  color_t mask_color = sprite->transparentColor();

  if (ink->isSelection() || ink->isSlice()) {
    m_type = SELECTION_CROSS;
  }
  else if (
    // Use cursor bounds for inks that are effects (eraser, blur, etc.)
    (ink->isEffect()) ||
    // or when the brush color is transparent and we are not in the background layer
    (layer && !layer->isBackground() &&
     brush_color == mask_color)) {
    m_type = BRUSH_BOUNDARIES;
  }
  else {
    m_type = CROSS;
  }

  // For cursor type 'bounds' we have to generate cursor boundaries
  if (m_type & BRUSH_BOUNDARIES)
    generateBoundaries();

  // Draw pixel/brush preview
  if ((m_type & CROSS) && m_editor->getState()->requireBrushPreview()) {
    Brush* brush = getCurrentBrush();
    gfx::Rect brushBounds = brush->bounds();
    brushBounds.offset(spritePos);

    // Create the extra cel to show the brush preview
    Site site = m_editor->getSite();
    Cel* cel = site.cel();

    int t, opacity = 255;
    if (cel) opacity = MUL_UN8(opacity, cel->opacity(), t);
    if (layer) opacity = MUL_UN8(opacity, static_cast<LayerImage*>(layer)->opacity(), t);

    document->prepareExtraCel(brushBounds, opacity);
    document->setExtraCelType(render::ExtraType::NONE);
    document->setExtraCelBlendMode(
      (layer ? static_cast<LayerImage*>(layer)->blendMode():
               BlendMode::NORMAL));

    Image* extraImage = document->getExtraCelImage();
    extraImage->setMaskColor(mask_color);
    clear_image(extraImage, mask_color);

    if (layer) {
      render::Render().renderLayer(
        extraImage, layer, m_editor->frame(),
        gfx::Clip(0, 0, brushBounds),
        BlendMode::SRC);

      // This extra cel is a patch for the current layer/frame
      document->setExtraCelType(render::ExtraType::PATCH);
    }

    tools::ToolLoop* loop = create_tool_loop_preview(
      m_editor, UIContext::instance(), extraImage,
      -gfx::Point(brushBounds.x,
                  brushBounds.y));

    if (loop) {
      loop->getInk()->prepareInk(loop);
      loop->getIntertwine()->prepareIntertwine();
      loop->getController()->prepareController();
      loop->getPointShape()->preparePointShape(loop);
      loop->getPointShape()->transformPoint(
        loop, -brush->bounds().x, -brush->bounds().y);
      delete loop;
    }

    document->notifySpritePixelsModified(
      sprite, gfx::Region(m_lastBounds = brushBounds));
  }

  // Save area and draw the cursor
  {
    ui::ScreenGraphics g;
    ui::SetClip clip(&g, gfx::Rect(0, 0, g.width(), g.height()));

    forEachBrushPixel(&g, m_screenPosition, spritePos, ui_cursor_color, &BrushPreview::savePixelDelegate);
    forEachBrushPixel(&g, m_screenPosition, spritePos, ui_cursor_color, &BrushPreview::drawPixelDelegate);
  }

  // Cursor in the editor (model)
  m_onScreen = true;
  m_editorPosition = spritePos;

  // Save the clipping-region to know where to clean the pixels
  m_oldClippingRegion = m_clippingRegion;
}

// Cleans the brush cursor from the specified editor.
//
// The mouse position is got from the last call to drawBrushPreview()
// (m_cursorEditor). So you must to use this routine only if you
// called drawBrushPreview() before.
void BrushPreview::hide()
{
  if (!m_onScreen)
    return;

  app::Document* document = m_editor->document();
  Sprite* sprite = m_editor->sprite();
  ASSERT(sprite);

  m_editor->getDrawableRegion(m_clippingRegion, ui::Widget::kCutTopWindows);

  gfx::Point pos = m_editorPosition;

  {
    // Restore pixels
    ui::ScreenGraphics g;
    ui::SetClip clip(&g, gfx::Rect(0, 0, g.width(), g.height()));

    forEachBrushPixel(&g, m_screenPosition, pos, gfx::ColorNone,
                      &BrushPreview::clearPixelDelegate);
  }

  // Clean pixel/brush preview
  if ((m_type & CROSS) &&
      m_editor->getState()->requireBrushPreview()) {
    document->destroyExtraCel();
    document->notifySpritePixelsModified(
      sprite, gfx::Region(m_lastBounds));
  }

  m_onScreen = false;
  m_clippingRegion.clear();
  m_oldClippingRegion.clear();
}

void BrushPreview::redraw()
{
  if (m_onScreen) {
    gfx::Point screenPos = m_screenPosition;
    hide();
    show(screenPos);
  }
}

void BrushPreview::invalidateRegion(const gfx::Region& region)
{
  m_clippingRegion.createSubtraction(m_clippingRegion, region);
}

void BrushPreview::generateBoundaries()
{
  Brush* brush = getCurrentBrush();

  if (m_brushBoundaries &&
      m_brushGen == brush->gen())
    return;

  Image* brushImage = brush->image();
  int w = brushImage->width();
  int h = brushImage->height();

  m_brushGen = brush->gen();
  m_brushWidth = w;
  m_brushHeight = h;

  ImageRef mask;
  if (brushImage->pixelFormat() != IMAGE_BITMAP) {
    mask.reset(Image::create(IMAGE_BITMAP, w, h));

    LockImageBits<BitmapTraits> bits(mask.get());
    auto pos = bits.begin();
    for (int v=0; v<h; ++v) {
      for (int u=0; u<w; ++u) {
        *pos = get_pixel(brushImage, u, v);
        ++pos;
      }
    }
  }

  m_brushBoundaries.reset(
    new MaskBoundaries(
      (mask ? mask.get(): brushImage)));
}

void BrushPreview::forEachBrushPixel(
  ui::Graphics* g,
  const gfx::Point& screenPos,
  const gfx::Point& spritePos,
  gfx::Color color,
  PixelDelegate pixelDelegate)
{
  m_savedPixelsIterator = 0;

  if (m_type & CROSS)
    traceCrossPixels(g, screenPos, color, pixelDelegate);

  if (m_type & SELECTION_CROSS)
    traceSelectionCrossPixels(g, spritePos, color, 1, pixelDelegate);

  if (m_type & BRUSH_BOUNDARIES)
    traceBrushBoundaries(g, spritePos, color, pixelDelegate);

  // Depending on the editor zoom, maybe wee need subpixel movement (a
  // little dot inside the active pixel)
  if (m_editor->zoom().scale() >= 4.0)
    (this->*pixelDelegate)(g, screenPos, color);
}

void BrushPreview::traceCrossPixels(
  ui::Graphics* g,
  const gfx::Point& pt, gfx::Color color,
  PixelDelegate pixelDelegate)
{
  static int cross[7*7] = {
    0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 1, 1,
    0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0,
  };
  gfx::Point out;
  int u, v;

  for (v=0; v<7; v++) {
    for (u=0; u<7; u++) {
      if (cross[v*7+u]) {
        out.x = pt.x-3+u;
        out.y = pt.y-3+v;
        (this->*pixelDelegate)(g, out, color);
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////
// Old Thick Cross

void BrushPreview::traceSelectionCrossPixels(
  ui::Graphics* g,
  const gfx::Point& pt, gfx::Color color,
  int thickness, PixelDelegate pixelDelegate)
{
  static int cross[6*6] = {
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0,
    1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1,
    0, 0, 1, 1, 0, 0,
    0, 0, 1, 1, 0, 0,
  };
  gfx::Point out, outpt = m_editor->editorToScreen(pt);
  int u, v;
  int size = m_editor->zoom().apply(thickness/2);
  int size2 = m_editor->zoom().apply(thickness);
  if (size2 == 0) size2 = 1;

  for (v=0; v<6; v++) {
    for (u=0; u<6; u++) {
      if (!cross[v*6+u])
        continue;

      out = outpt;
      out.x += ((u<3) ? u-size-3: u-size-3+size2);
      out.y += ((v<3) ? v-size-3: v-size-3+size2);

      (this->*pixelDelegate)(g, out, color);
    }
  }
}

//////////////////////////////////////////////////////////////////////
// Current Brush Bounds

void BrushPreview::traceBrushBoundaries(ui::Graphics* g,
                                        gfx::Point pos,
                                        gfx::Color color,
                                        PixelDelegate pixelDelegate)
{
  pos.x -= m_brushWidth/2;
  pos.y -= m_brushHeight/2;

  for (const auto& seg : *m_brushBoundaries) {
    gfx::Rect bounds = seg.bounds();
    bounds.offset(pos);
    bounds = m_editor->editorToScreen(bounds);

    if (seg.open()) {
      if (seg.vertical()) --bounds.x;
      else --bounds.y;
    }

    gfx::Point pt(bounds.x, bounds.y);
    if (seg.vertical()) {
      for (; pt.y<bounds.y+bounds.h; ++pt.y)
        (this->*pixelDelegate)(g, pt, color);
    }
    else {
      for (; pt.x<bounds.x+bounds.w; ++pt.x)
        (this->*pixelDelegate)(g, pt, color);
    }
  }
}

void BrushPreview::savePixelDelegate(ui::Graphics* g, const gfx::Point& pt, gfx::Color color)
{
  if (m_clippingRegion.contains(pt)) {
    color_t c = g->getPixel(pt.x, pt.y);

    if (m_savedPixelsIterator < (int)m_savedPixels.size())
      m_savedPixels[m_savedPixelsIterator] = c;
    else
      m_savedPixels.push_back(c);

    ++m_savedPixelsIterator;
  }
}

void BrushPreview::drawPixelDelegate(ui::Graphics* gfx, const gfx::Point& pt, gfx::Color color)
{
  if (m_savedPixelsIterator < (int)m_savedPixels.size() &&
      m_clippingRegion.contains(pt)) {
    if (m_blackAndWhiteNegative) {
      int c = m_savedPixels[m_savedPixelsIterator++];
      int r = gfx::getr(c);
      int g = gfx::getg(c);
      int b = gfx::getb(c);

      gfx->putPixel(color_utils::blackandwhite_neg(gfx::rgba(r, g, b)), pt.x, pt.y);
    }
    else {
      gfx->putPixel(color, pt.x, pt.y);
    }
  }
}

void BrushPreview::clearPixelDelegate(ui::Graphics* g, const gfx::Point& pt, gfx::Color color)
{
  if (m_savedPixelsIterator < (int)m_savedPixels.size()) {
    if (m_clippingRegion.contains(pt))
      g->putPixel(m_savedPixels[m_savedPixelsIterator++], pt.x, pt.y);
    else if (!m_oldClippingRegion.isEmpty() &&
             m_oldClippingRegion.contains(pt))
      m_savedPixelsIterator++;
  }
}

} // namespace app
