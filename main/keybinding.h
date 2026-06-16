#pragma once

#include "editor_core.h"
#include "key_event.h"
#include "text_input_composer.h"

#include <string>

struct KeybindingResult {
  bool document_changed = false;
  bool cursor_changed = false;
  bool input_mode_changed = false;
  bool composing_changed = false;
  bool toast_changed = false;
  bool snapshot_requested = false;
  std::string toast;
};

KeybindingResult keybinding_dispatch(EditorCore &editor,
                                      TextInputComposer &composer,
                                      const KeyEvent &event);
