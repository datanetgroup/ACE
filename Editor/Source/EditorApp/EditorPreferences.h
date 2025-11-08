#pragma once
#include <optional>

struct EditorState;

namespace ace::editor
{
    // Open/close & draw the Preferences window each frame.
    void OpenEditorPreferences();
    void DrawEditorPreferences(EditorState& S);
}
