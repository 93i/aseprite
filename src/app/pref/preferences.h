// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifndef APP_PREF_PREFERENCES_H_INCLUDED
#define APP_PREF_PREFERENCES_H_INCLUDED
#pragma once

#include "app/color.h"
#include "app/pref/option.h"
#include "app/tools/freehand_algorithm.h"
#include "app/tools/ink_type.h"
#include "app/tools/rotation_algorithm.h"
#include "app/tools/selection_mode.h"
#include "doc/anidir.h"
#include "doc/brush_pattern.h"
#include "doc/documents_observer.h"
#include "doc/frame.h"
#include "doc/layer_index.h"
#include "filters/tiled_mode.h"
#include "gfx/rect.h"

#include "generated_pref_types.h"

#include <map>
#include <string>
#include <vector>

namespace app {
  namespace tools {
    class Tool;
  }

  class Document;

  typedef app::gen::ToolPref ToolPreferences;
  typedef app::gen::DocPref DocumentPreferences;

  class Preferences : public app::gen::GlobalPref
                    , public doc::DocumentsObserver {
  public:
    static Preferences& instance();

    Preferences();
    ~Preferences();

    void load();
    void save();

    ToolPreferences& tool(tools::Tool* tool);
    DocumentPreferences& document(const app::Document* doc);

  protected:
    void onRemoveDocument(doc::Document* doc) override;

  private:
    std::string docConfigFileName(const app::Document* doc);

    void serializeDocPref(const app::Document* doc, app::DocumentPreferences* docPref, bool save);

    std::map<std::string, app::ToolPreferences*> m_tools;
    std::map<const app::Document*, app::DocumentPreferences*> m_docs;
  };

} // namespace app

#endif
