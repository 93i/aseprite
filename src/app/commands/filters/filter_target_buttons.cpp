// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/filters/filter_target_buttons.h"

#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/ui/skin/skin_parts.h"
#include "base/bind.h"
#include "doc/image.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/theme.h"
#include "ui/widget.h"

#include <cstring>

namespace app {

using namespace filters;
using namespace ui;
using namespace app::skin;

FilterTargetButtons::FilterTargetButtons(int imgtype, bool withChannels)
  : Box(VERTICAL)
  , m_target(0)
{
#define ADD(box, widget, hook)                                          \
  if (widget) {                                                         \
    widget->setBorder(gfx::Border(2 * guiscale()));                    \
    box->addChild(widget);                                              \
    widget->Click.connect(Bind<void>(&FilterTargetButtons::hook, this, widget)); \
  }

  Box* hbox;
  CheckBox* r = NULL;
  CheckBox* g = NULL;
  CheckBox* b = NULL;
  CheckBox* k = NULL;
  CheckBox* a = NULL;
  CheckBox* index = NULL;
  Button* images = NULL;

  hbox = new Box(HORIZONTAL | HOMOGENEOUS);

  this->noBorderNoChildSpacing();
  hbox->noBorderNoChildSpacing();

  if (withChannels) {
    switch (imgtype) {

      case IMAGE_RGB:
      case IMAGE_INDEXED:
        r = check_button_new("R", 2, 0, 0, 0);
        g = check_button_new("G", 0, 0, 0, 0);
        b = check_button_new("B", 0, (imgtype == IMAGE_RGB) ? 0: 2, 0, 0);

        r->setId("r");
        g->setId("g");
        b->setId("b");

        if (imgtype == IMAGE_RGB) {
          a = check_button_new("A", 0, 2, 0, 0);
          a->setId("a");
        }
        else {
          index = check_button_new("Index", 0, 0, 0, 0);
          index->setId("i");
        }
        break;

      case IMAGE_GRAYSCALE:
        k = check_button_new("K", 2, 0, 0, 0);
        a = check_button_new("A", 0, 2, 0, 0);

        k->setId("k");
        a->setId("a");
        break;
    }
  }

  // Create the button to select "image" target
  images = new Button("");
  setup_bevels(images,
               withChannels ? 0: 2,
               withChannels ? 0: 2, 2, 2);
  setup_mini_look(images);
  set_gfxicon_to_button(images,
                        getTargetNormalIcon(),
                        getTargetSelectedIcon(), -1,
                        CENTER | MIDDLE);

  // Make hierarchy
  ADD(hbox, r, onChannelChange);
  ADD(hbox, g, onChannelChange);
  ADD(hbox, b, onChannelChange);
  ADD(hbox, k, onChannelChange);
  ADD(hbox, a, onChannelChange);

  if (withChannels)
    addChild(hbox);
  else
    delete hbox;

  ADD(this, index, onChannelChange);
  ADD(this, images, onImagesChange);
}

void FilterTargetButtons::setTarget(int target)
{
  m_target = target;

  selectTargetButton("r", TARGET_RED_CHANNEL);
  selectTargetButton("g", TARGET_GREEN_CHANNEL);
  selectTargetButton("b", TARGET_BLUE_CHANNEL);
  selectTargetButton("a", TARGET_ALPHA_CHANNEL);
  selectTargetButton("k", TARGET_GRAY_CHANNEL);
  selectTargetButton("i", TARGET_INDEX_CHANNEL);
}

void FilterTargetButtons::selectTargetButton(const char* name, int specificTarget)
{
  Widget* wgt = findChild(name);
  if (wgt != NULL) {
    wgt->setSelected((m_target & specificTarget) == specificTarget);
  }
}

void FilterTargetButtons::onChannelChange(ButtonBase* button)
{
  int flag = 0;

  switch (button->getId()[0]) {
    case 'r': flag = TARGET_RED_CHANNEL; break;
    case 'g': flag = TARGET_GREEN_CHANNEL; break;
    case 'b': flag = TARGET_BLUE_CHANNEL; break;
    case 'k': flag = TARGET_GRAY_CHANNEL; break;
    case 'a': flag = TARGET_ALPHA_CHANNEL; break;
    case 'i': flag = TARGET_INDEX_CHANNEL; break;
    default:
      return;
  }

  if (button->isSelected())
    m_target |= flag;
  else
    m_target &= ~flag;

  TargetChange();
}

void FilterTargetButtons::onImagesChange(ButtonBase* button)
{
  // Rotate target
  if (m_target & TARGET_ALL_FRAMES) {
    m_target &= ~TARGET_ALL_FRAMES;

    if (m_target & TARGET_ALL_LAYERS)
      m_target &= ~TARGET_ALL_LAYERS;
    else
      m_target |= TARGET_ALL_LAYERS;
  }
  else {
    m_target |= TARGET_ALL_FRAMES;
  }

  set_gfxicon_to_button(button,
                        getTargetNormalIcon(),
                        getTargetSelectedIcon(), -1,
                        CENTER | MIDDLE);

  TargetChange();
}

int FilterTargetButtons::getTargetNormalIcon() const
{
  if (m_target & TARGET_ALL_FRAMES) {
    return (m_target & TARGET_ALL_LAYERS) ?
      PART_TARGET_FRAMES_LAYERS:
      PART_TARGET_FRAMES;
  }
  else {
    return (m_target & TARGET_ALL_LAYERS) ?
      PART_TARGET_LAYERS:
      PART_TARGET_ONE;
  }
}

int FilterTargetButtons::getTargetSelectedIcon() const
{
  if (m_target & TARGET_ALL_FRAMES) {
    return (m_target & TARGET_ALL_LAYERS) ?
      PART_TARGET_FRAMES_LAYERS_SELECTED:
      PART_TARGET_FRAMES_SELECTED;
  }
  else {
    return (m_target & TARGET_ALL_LAYERS) ?
      PART_TARGET_LAYERS_SELECTED:
      PART_TARGET_ONE_SELECTED;
  }
}

} // namespace app
