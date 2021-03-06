// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_UI_EDITOR_BRUSH_PREVIEW_H_INCLUDED
#define APP_UI_EDITOR_BRUSH_PREVIEW_H_INCLUDED
#pragma once

#include "base/shared_ptr.h"
#include "doc/color.h"
#include "doc/mask_boundaries.h"
#include "gfx/color.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "gfx/region.h"

#include <vector>

namespace doc {
  class Brush;
  class Layer;
  class Sprite;
}

namespace ui {
  class Graphics;
}

namespace app {
  class Editor;

  class BrushPreview {
  public:
    // Brush type
    enum {
      CROSS            = 1,
      SELECTION_CROSS  = 2,
      BRUSH_BOUNDARIES = 4,
    };

    BrushPreview(Editor* editor);
    ~BrushPreview();

    bool onScreen() const { return m_onScreen; }
    const gfx::Point& screenPosition() const { return m_screenPosition; }

    void show(const gfx::Point& screenPos);
    void move(const gfx::Point& screenPos);
    void hide();
    void redraw();

    void invalidateRegion(const gfx::Region& region);

  private:
    typedef void (BrushPreview::*PixelDelegate)(ui::Graphics*, const gfx::Point&, gfx::Color);

    static doc::Brush* getCurrentBrush();
    static doc::color_t getBrushColor(doc::Sprite* sprite, doc::Layer* layer);

    void generateBoundaries();
    void forEachBrushPixel(
      ui::Graphics* g,
      const gfx::Point& screenPos,
      const gfx::Point& spritePos,
      gfx::Color color,
      PixelDelegate pixelDelegate);

    void traceCrossPixels(ui::Graphics* g, const gfx::Point& pt, gfx::Color color, PixelDelegate pixel);
    void traceSelectionCrossPixels(ui::Graphics* g, const gfx::Point& pt, gfx::Color color, int thickness, PixelDelegate pixel);
    void traceBrushBoundaries(ui::Graphics* g, gfx::Point pos, gfx::Color color, PixelDelegate pixel);

    void savePixelDelegate(ui::Graphics* g, const gfx::Point& pt, gfx::Color color);
    void drawPixelDelegate(ui::Graphics* g, const gfx::Point& pt, gfx::Color color);
    void clearPixelDelegate(ui::Graphics* g, const gfx::Point& pt, gfx::Color color);

    Editor* m_editor;
    int m_type;

    // The brush preview shows the cross or brush boundaries as black
    // & white negative.
    bool m_blackAndWhiteNegative;

    // The brush preview is on the screen.
    bool m_onScreen;
    gfx::Point m_screenPosition; // Position in the screen (view)
    gfx::Point m_editorPosition; // Position in the editor (model)

    // Information about current brush
    base::SharedPtr<doc::MaskBoundaries> m_brushBoundaries;
    int m_brushGen;
    int m_brushWidth;
    int m_brushHeight;

    static std::vector<gfx::Color> m_savedPixels;
    static int m_savedPixelsIterator;

    gfx::Region m_clippingRegion;
    gfx::Region m_oldClippingRegion;

    gfx::Rect m_lastBounds;
  };

  class HideBrushPreview {
  public:
    HideBrushPreview(BrushPreview& brushPreview)
      : m_brushPreview(brushPreview)
      , m_oldScreenPosition(brushPreview.screenPosition())
      , m_onScreen(brushPreview.onScreen()) {
      if (m_onScreen)
        m_brushPreview.hide();
    }

    ~HideBrushPreview() {
      if (m_onScreen)
        m_brushPreview.show(m_oldScreenPosition);
    }

  private:
    BrushPreview& m_brushPreview;
    gfx::Point m_oldScreenPosition;
    bool m_onScreen;
  };

} // namespace app

#endif
