#include "EditorSettingsPanel.h"
#include "imgui.h"

// one window helper
static bool BeginSettingsWindow(const char* title, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
    return ImGui::Begin(title, open, ImGuiWindowFlags_NoCollapse);
}

// static flags (kept here so we don't need to touch EditorState yet)
static bool g_EditorPreferences = false;
static bool g_Plugins = false;

static bool g_Settings_Input = false;
static bool g_Settings_Rendering = false;
static bool g_Settings_Physics = false;
static bool g_Settings_Audio = false;
static bool g_Settings_Scripting = false;
static bool g_Settings_Networking = false;
static bool g_Settings_Online = false;
static bool g_Settings_AssetManager = false;
static bool g_Settings_Gameplay = false;
static bool g_Settings_ContentPaths = false;
static bool g_Settings_Localization = false;
static bool g_Settings_Build = false;
static bool g_Settings_Platforms = false;
static bool g_Settings_SourceControl = false;
static bool g_Settings_Appearance = false;
static bool g_Settings_Keymap = false;
static bool g_Settings_Diagnostics = false;
static bool g_Settings_Memory = false;
static bool g_Settings_CVars = false;

// accessor impl
bool* Flag_EditorPreferences() { return &g_EditorPreferences; }
bool* Flag_Plugins() { return &g_Plugins; }

bool* Flag_Settings_Input() { return &g_Settings_Input; }
bool* Flag_Settings_Rendering() { return &g_Settings_Rendering; }
bool* Flag_Settings_Physics() { return &g_Settings_Physics; }
bool* Flag_Settings_Audio() { return &g_Settings_Audio; }
bool* Flag_Settings_Scripting() { return &g_Settings_Scripting; }
bool* Flag_Settings_Networking() { return &g_Settings_Networking; }
bool* Flag_Settings_Online() { return &g_Settings_Online; }
bool* Flag_Settings_AssetManager() { return &g_Settings_AssetManager; }
bool* Flag_Settings_Gameplay() { return &g_Settings_Gameplay; }
bool* Flag_Settings_ContentPaths() { return &g_Settings_ContentPaths; }
bool* Flag_Settings_Localization() { return &g_Settings_Localization; }
bool* Flag_Settings_Build() { return &g_Settings_Build; }
bool* Flag_Settings_Platforms() { return &g_Settings_Platforms; }
bool* Flag_Settings_SourceControl() { return &g_Settings_SourceControl; }
bool* Flag_Settings_Appearance() { return &g_Settings_Appearance; }
bool* Flag_Settings_Keymap() { return &g_Settings_Keymap; }
bool* Flag_Settings_Diagnostics() { return &g_Settings_Diagnostics; }
bool* Flag_Settings_Memory() { return &g_Settings_Memory; }
bool* Flag_Settings_CVars() { return &g_Settings_CVars; }

// forward decl of per-panel drawers (simple stubs)
static void DrawEditorPreferences();
static void DrawPluginManager();

static void DrawSettings_Input();
static void DrawSettings_Rendering();
static void DrawSettings_Physics();
static void DrawSettings_Audio();
static void DrawSettings_Scripting();
static void DrawSettings_Networking();
static void DrawSettings_Online();
static void DrawSettings_AssetManager();
static void DrawSettings_Gameplay();
static void DrawSettings_ContentPaths();
static void DrawSettings_Localization();
static void DrawSettings_Build();
static void DrawSettings_Platforms();
static void DrawSettings_SourceControl();
static void DrawSettings_Appearance();
static void DrawSettings_Keymap();
static void DrawSettings_Diagnostics();
static void DrawSettings_Memory();
static void DrawSettings_CVars();

void DrawAllSettingsPanels(EditorState& /*S*/) {
    // Editor
    if (g_EditorPreferences)   DrawEditorPreferences();
    if (g_Plugins)             DrawPluginManager();

    // Project
    if (g_Settings_Input)          DrawSettings_Input();
    if (g_Settings_Rendering)      DrawSettings_Rendering();
    if (g_Settings_Physics)        DrawSettings_Physics();
    if (g_Settings_Audio)          DrawSettings_Audio();
    if (g_Settings_Scripting)      DrawSettings_Scripting();
    if (g_Settings_Networking)     DrawSettings_Networking();
    if (g_Settings_Online)         DrawSettings_Online();
    if (g_Settings_AssetManager)   DrawSettings_AssetManager();
    if (g_Settings_Gameplay)       DrawSettings_Gameplay();
    if (g_Settings_ContentPaths)   DrawSettings_ContentPaths();
    if (g_Settings_Localization)   DrawSettings_Localization();
    if (g_Settings_Build)          DrawSettings_Build();
    if (g_Settings_Platforms)      DrawSettings_Platforms();
    if (g_Settings_SourceControl)  DrawSettings_SourceControl();
    if (g_Settings_Appearance)     DrawSettings_Appearance();
    if (g_Settings_Keymap)         DrawSettings_Keymap();
    if (g_Settings_Diagnostics)    DrawSettings_Diagnostics();
    if (g_Settings_Memory)         DrawSettings_Memory();
    if (g_Settings_CVars)          DrawSettings_CVars();
}

// ===== Stub Panels =====

static void DrawEditorPreferences() {
    if (BeginSettingsWindow("Editor Preferences", &g_EditorPreferences)) {
        ImGui::TextDisabled("TODO: Editor Preferences");
        ImGui::Separator();
        ImGui::Text("Example toggles go here.");
        ImGui::End();
    }
}
static void DrawPluginManager() {
    if (BeginSettingsWindow("Plugins", &g_Plugins)) {
        ImGui::TextDisabled("TODO: Plugin discovery, enable/disable, reload");
        ImGui::End();
    }
}

static void DrawSettings_Input() {
    if (BeginSettingsWindow("Project Settings - Input", &g_Settings_Input)) {
        ImGui::TextDisabled("TODO: Action/Axis maps, device profiles.");
        ImGui::End();
    }
}
static void DrawSettings_Rendering() {
    if (BeginSettingsWindow("Project Settings - Rendering", &g_Settings_Rendering)) {
        ImGui::TextDisabled("TODO: Renderer backend, vsync, MSAA, post-process toggles.");
        ImGui::End();
    }
}
static void DrawSettings_Physics() {
    if (BeginSettingsWindow("Project Settings - Physics", &g_Settings_Physics)) {
        ImGui::TextDisabled("TODO: Timestep, gravity, layers, materials.");
        ImGui::End();
    }
}
static void DrawSettings_Audio() {
    if (BeginSettingsWindow("Project Settings - Audio", &g_Settings_Audio)) {
        ImGui::TextDisabled("TODO: Master bus, device, sample rate.");
        ImGui::End();
    }
}
static void DrawSettings_Scripting() {
    if (BeginSettingsWindow("Project Settings - Scripting / Visual Graph", &g_Settings_Scripting)) {
        ImGui::TextDisabled("TODO: Visual graph settings, hot-reload.");
        ImGui::End();
    }
}
static void DrawSettings_Networking() {
    if (BeginSettingsWindow("Project Settings - Networking & Multiplayer", &g_Settings_Networking)) {
        ImGui::TextDisabled("TODO: Ports, max players, replication settings.");
        ImGui::End();
    }
}
static void DrawSettings_Online() {
    if (BeginSettingsWindow("Project Settings - Online Services", &g_Settings_Online)) {
        ImGui::TextDisabled("TODO: Steam, PlayFab, auth options.");
        ImGui::End();
    }
}
static void DrawSettings_AssetManager() {
    if (BeginSettingsWindow("Project Settings - Asset Manager", &g_Settings_AssetManager)) {
        ImGui::TextDisabled("TODO: Mount points, primary asset rules.");
        ImGui::End();
    }
}
static void DrawSettings_Gameplay() {
    if (BeginSettingsWindow("Project Settings - Gameplay (Tags / Globals)", &g_Settings_Gameplay)) {
        ImGui::TextDisabled("TODO: Global tags, difficulty, game rules.");
        ImGui::End();
    }
}
static void DrawSettings_ContentPaths() {
    if (BeginSettingsWindow("Project Settings - Content Paths & Mounts", &g_Settings_ContentPaths)) {
        ImGui::TextDisabled("TODO: Root content dirs, virtual mounts.");
        ImGui::End();
    }
}
static void DrawSettings_Localization() {
    if (BeginSettingsWindow("Project Settings - Localization", &g_Settings_Localization)) {
        ImGui::TextDisabled("TODO: Locales, export/import, fallback.");
        ImGui::End();
    }
}
static void DrawSettings_Build() {
    if (BeginSettingsWindow("Project Settings - Build & Packaging", &g_Settings_Build)) {
        ImGui::TextDisabled("TODO: Targets, configs, symbols, strip options.");
        ImGui::End();
    }
}
static void DrawSettings_Platforms() {
    if (BeginSettingsWindow("Project Settings - Platforms", &g_Settings_Platforms)) {
        ImGui::TextDisabled("TODO: Per-platform overrides.");
        ImGui::End();
    }
}
static void DrawSettings_SourceControl() {
    if (BeginSettingsWindow("Editor Settings - Source Control", &g_Settings_SourceControl)) {
        ImGui::TextDisabled("TODO: Provider, connection, auto-submit toggles.");
        ImGui::End();
    }
}
static void DrawSettings_Appearance() {
    if (BeginSettingsWindow("Editor Settings - Theme & Layout", &g_Settings_Appearance)) {
        ImGui::TextDisabled("TODO: Theme, font size, docking presets.");
        ImGui::End();
    }
}
static void DrawSettings_Keymap() {
    if (BeginSettingsWindow("Editor Settings - Keymap & Shortcuts", &g_Settings_Keymap)) {
        ImGui::TextDisabled("TODO: Bindings, chord detection, export/import.");
        ImGui::End();
    }
}
static void DrawSettings_Diagnostics() {
    if (BeginSettingsWindow("Editor Settings - Diagnostics", &g_Settings_Diagnostics)) {
        ImGui::TextDisabled("TODO: Logging levels, telemetry opt-in.");
        ImGui::End();
    }
}
static void DrawSettings_Memory() {
    if (BeginSettingsWindow("Project Settings - GC & Memory", &g_Settings_Memory)) {
        ImGui::TextDisabled("TODO: GC thresholds, pooling, leak checks.");
        ImGui::End();
    }
}
static void DrawSettings_CVars() {
    if (BeginSettingsWindow("Settings - Console Variables (CVars)", &g_Settings_CVars)) {
        ImGui::TextDisabled("TODO: Live CVar browser, filter, favorites.");
        ImGui::End();
    }
}
