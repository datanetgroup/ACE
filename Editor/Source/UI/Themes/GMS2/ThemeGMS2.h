#pragma once
// GMS2-inspired styling for Dear ImGui + imnodes + TextEditor,
// plus a default IDE-like DockSpace & layout.
//
// Requirements:
//  - Dear ImGui (docking)
//  - Optional: imnodes (for graph editor skin)
//  - Optional: BalazsJako/ImGuiColorTextEdit (TextEditor)

struct ImGuiIO;
typedef unsigned int ImGuiID;

// Forward declare the real editor type in the global namespace
struct TextEditor;

namespace ace::ui::themes::gms2
{
    // Core ImGui theme (colors, spacing, rounding, flags)
    void ApplyBase(float scale = 1.0f);
    void ApplyBaseLight(float scale = 1.0f);

    // Main fullscreen host + DockSpace. Call once per frame before your panels.
    void BeginMainDockspace(const char* name = "ACE Editor", bool* pOpen = nullptr);

    // Rebuild the default split layout programmatically.
    void RebuildDefaultLayout();

    // ---- Optional integrations (no-ops if the libs aren't in your project) ----
    void ApplyGraphTheme(); // imnodes
    void ApplyGraphThemeLight();


    // Use the global TextEditor type
    void ApplyTextEditorTheme(::TextEditor& editor);
    void ApplyTextEditorThemeLight(::TextEditor& editor);
}
