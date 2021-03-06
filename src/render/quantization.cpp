// Aseprite Render Library
// Copyright (c) 2001-2015 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "render/quantization.h"

#include "doc/image_impl.h"
#include "doc/images_collector.h"
#include "doc/layer.h"
#include "doc/palette.h"
#include "doc/primitives.h"
#include "doc/rgbmap.h"
#include "doc/sprite.h"
#include "gfx/hsv.h"
#include "gfx/rgb.h"
#include "render/ordered_dither.h"
#include "render/render.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

namespace render {

using namespace doc;
using namespace gfx;

Palette* create_palette_from_rgb(
  const Sprite* sprite,
  frame_t fromFrame,
  frame_t toFrame,
  Palette* palette)
{
  PaletteOptimizer optimizer;

  if (!palette)
    palette = new Palette(fromFrame, 256);

  bool has_background_layer = (sprite->backgroundLayer() != nullptr);

  // Add a flat image with the current sprite's frame rendered
  ImageRef flat_image(Image::create(IMAGE_RGB,
      sprite->width(), sprite->height()));

  // Feed the optimizer with all rendered frames
  render::Render render;
  for (frame_t frame=fromFrame; frame<=toFrame; ++frame) {
    render.renderSprite(flat_image.get(), sprite, frame);
    optimizer.feedWithImage(flat_image.get());
  }

  // Generate an optimized palette
  optimizer.calculate(palette, has_background_layer);

  return palette;
}

Image* convert_pixel_format(
  const Image* image,
  Image* new_image,
  PixelFormat pixelFormat,
  DitheringMethod ditheringMethod,
  const RgbMap* rgbmap,
  const Palette* palette,
  bool is_background)
{
  if (!new_image)
    new_image = Image::create(pixelFormat, image->width(), image->height());

  // RGB -> Indexed with ordered dithering
  if (image->pixelFormat() == IMAGE_RGB &&
      pixelFormat == IMAGE_INDEXED &&
      ditheringMethod == DitheringMethod::ORDERED) {
    BayerMatrix<8> matrix;
    OrderedDither dither;
    dither.ditherRgbImageToIndexed(matrix, image, new_image, 0, 0, rgbmap, palette);
    return new_image;
  }

  color_t c;
  int r, g, b;

  switch (image->pixelFormat()) {

    case IMAGE_RGB: {
      const LockImageBits<RgbTraits> srcBits(image);
      LockImageBits<RgbTraits>::const_iterator src_it = srcBits.begin(), src_end = srcBits.end();

      switch (new_image->pixelFormat()) {

        // RGB -> RGB
        case IMAGE_RGB:
          new_image->copy(image, gfx::Clip(image->bounds()));
          break;

        // RGB -> Grayscale
        case IMAGE_GRAYSCALE: {
          LockImageBits<GrayscaleTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<GrayscaleTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<GrayscaleTraits>::iterator dst_end = dstBits.end();
#endif

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            g = 255 * Hsv(Rgb(rgba_getr(c),
                              rgba_getg(c),
                              rgba_getb(c))).valueInt() / 100;

            *dst_it = graya(g, rgba_geta(c));
          }
          ASSERT(dst_it == dst_end);
          break;
        }

        // RGB -> Indexed
        case IMAGE_INDEXED: {
          LockImageBits<IndexedTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<IndexedTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<IndexedTraits>::iterator dst_end = dstBits.end();
#endif

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            r = rgba_getr(c);
            g = rgba_getg(c);
            b = rgba_getb(c);

            if (rgba_geta(c) == 0)
              *dst_it = 0;
            else
              *dst_it = rgbmap->mapColor(r, g, b);
          }
          ASSERT(dst_it == dst_end);
          break;
        }
      }
      break;
    }

    case IMAGE_GRAYSCALE: {
      const LockImageBits<GrayscaleTraits> srcBits(image);
      LockImageBits<GrayscaleTraits>::const_iterator src_it = srcBits.begin(), src_end = srcBits.end();

      switch (new_image->pixelFormat()) {

        // Grayscale -> RGB
        case IMAGE_RGB: {
          LockImageBits<RgbTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<RgbTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<RgbTraits>::iterator dst_end = dstBits.end();
#endif

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            g = graya_getv(c);

            *dst_it = rgba(g, g, g, graya_geta(c));
          }
          ASSERT(dst_it == dst_end);
          break;
        }

        // Grayscale -> Grayscale
        case IMAGE_GRAYSCALE:
          new_image->copy(image, gfx::Clip(image->bounds()));
          break;

        // Grayscale -> Indexed
        case IMAGE_INDEXED: {
          LockImageBits<IndexedTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<IndexedTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<IndexedTraits>::iterator dst_end = dstBits.end();
#endif

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            if (graya_geta(c) == 0)
              *dst_it = 0;
            else
              *dst_it = graya_getv(c);
          }
          ASSERT(dst_it == dst_end);
          break;
        }
      }
      break;
    }

    case IMAGE_INDEXED: {
      const LockImageBits<IndexedTraits> srcBits(image);
      LockImageBits<IndexedTraits>::const_iterator src_it = srcBits.begin(), src_end = srcBits.end();

      switch (new_image->pixelFormat()) {

        // Indexed -> RGB
        case IMAGE_RGB: {
          LockImageBits<RgbTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<RgbTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<RgbTraits>::iterator dst_end = dstBits.end();
#endif

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            if (!is_background && c == image->maskColor())
              *dst_it = 0;
            else
              *dst_it = rgba(rgba_getr(palette->getEntry(c)),
                             rgba_getg(palette->getEntry(c)),
                             rgba_getb(palette->getEntry(c)), 255);
          }
          ASSERT(dst_it == dst_end);
          break;
        }

        // Indexed -> Grayscale
        case IMAGE_GRAYSCALE: {
          LockImageBits<GrayscaleTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<GrayscaleTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<GrayscaleTraits>::iterator dst_end = dstBits.end();
#endif

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            if (!is_background && c == image->maskColor())
              *dst_it = 0;
            else {
              r = rgba_getr(palette->getEntry(c));
              g = rgba_getg(palette->getEntry(c));
              b = rgba_getb(palette->getEntry(c));

              g = 255 * Hsv(Rgb(r, g, b)).valueInt() / 100;
              *dst_it = graya(g, 255);
            }
          }
          ASSERT(dst_it == dst_end);
          break;
        }

        // Indexed -> Indexed
        case IMAGE_INDEXED: {
          LockImageBits<IndexedTraits> dstBits(new_image, Image::WriteLock);
          LockImageBits<IndexedTraits>::iterator dst_it = dstBits.begin();
#ifdef _DEBUG
          LockImageBits<IndexedTraits>::iterator dst_end = dstBits.end();
#endif
          color_t dstMaskColor = new_image->maskColor();

          for (; src_it != src_end; ++src_it, ++dst_it) {
            ASSERT(dst_it != dst_end);
            c = *src_it;

            if (!is_background && c == image->maskColor())
              *dst_it = dstMaskColor;
            else {
              r = rgba_getr(palette->getEntry(c));
              g = rgba_getg(palette->getEntry(c));
              b = rgba_getb(palette->getEntry(c));

              *dst_it = rgbmap->mapColor(r, g, b);
            }
          }
          ASSERT(dst_it == dst_end);
          break;
        }

      }
      break;
    }
  }

  return new_image;
}

//////////////////////////////////////////////////////////////////////
// Creation of optimized palette for RGB images
// by David Capello

void PaletteOptimizer::feedWithImage(Image* image)
{
  uint32_t color;

  ASSERT(image);
  switch (image->pixelFormat()) {

    case IMAGE_RGB:
      {
        const LockImageBits<RgbTraits> bits(image);
        LockImageBits<RgbTraits>::const_iterator it = bits.begin(), end = bits.end();

        for (; it != end; ++it) {
          color = *it;

          if (rgba_geta(color) > 0) {
            color |= rgba(0, 0, 0, 255);
            m_histogram.addSamples(color, 1);
          }
        }
      }
      break;

    case IMAGE_GRAYSCALE:
      {
        const LockImageBits<RgbTraits> bits(image);
        LockImageBits<RgbTraits>::const_iterator it = bits.begin(), end = bits.end();

        for (; it != end; ++it) {
          color = *it;

          if (graya_geta(color) > 0) {
            color = graya_getv(color);
            m_histogram.addSamples(rgba(color, color, color, 255), 1);
          }
        }
      }
      break;

    case IMAGE_INDEXED:
      ASSERT(false);
      break;

  }
}

void PaletteOptimizer::calculate(Palette* palette, bool has_background_layer)
{
  // If the sprite has a background layer, the first entry can be
  // used, in other case the 0 indexed will be the mask color, so it
  // will not be used later in the color conversion (from RGB to
  // Indexed).
  int first_usable_entry = (has_background_layer ? 0: 1);
  int used_colors = m_histogram.createOptimizedPalette(
    palette, first_usable_entry, palette->size()-1);
  palette->resize(MAX(1, first_usable_entry+used_colors));
}

void create_palette_from_images(const std::vector<Image*>& images, Palette* palette, bool has_background_layer)
{
  PaletteOptimizer optimizer;
  for (int i=0; i<(int)images.size(); ++i)
    optimizer.feedWithImage(images[i]);

  optimizer.calculate(palette, has_background_layer);
}

} // namespace render
