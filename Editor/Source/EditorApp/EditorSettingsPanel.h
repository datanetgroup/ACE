#pragma once

struct EditorState;

// Flag accessors (return pointers so they can be used with ImGui::MenuItem)
bool* Flag_EditorPreferences();
bool* Flag_Plugins();

bool* Flag_Settings_Input();
bool* Flag_Settings_Rendering();
bool* Flag_Settings_Physics();
bool* Flag_Settings_Audio();
bool* Flag_Settings_Scripting();
bool* Flag_Settings_Networking();
bool* Flag_Settings_Online();
bool* Flag_Settings_AssetManager();
bool* Flag_Settings_Gameplay();
bool* Flag_Settings_ContentPaths();
bool* Flag_Settings_Localization();
bool* Flag_Settings_Build();
bool* Flag_Settings_Platforms();
bool* Flag_Settings_SourceControl();
bool* Flag_Settings_Appearance();
bool* Flag_Settings_Keymap();
bool* Flag_Settings_Diagnostics();
bool* Flag_Settings_Memory();
bool* Flag_Settings_CVars();

// Draw all settings panels (each only renders if its flag is true)
void DrawAllSettingsPanels(EditorState& S);
