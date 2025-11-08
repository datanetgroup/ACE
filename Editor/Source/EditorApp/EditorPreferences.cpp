#include "EditorPreferences.h"
#include "UI/Themes/ThemeManager.h"
#include "imgui.h"
#include <array>

namespace ace::editor
{
    static bool   g_ShowPrefs = false;
    static bool   g_ShowRestartModal = false;
    static bool   g_ShowRestartBanner = false;
    static int    g_SelectedSection = 0; // left nav

    struct Section { const char* Label; };
    static constexpr std::array<Section, 9> kSections{{
        {"General"},
        {"Theme & Layout"},   // renamed from "Appearance"
        {"Graph Editor"},
        {"Code Editor"},
        {"Projects"},
        {"Autosave"},
        {"Plugins"},
        {"Paths"},
        {"Scripting"},
    }};

    void OpenEditorPreferences() { g_ShowPrefs = true; }

    // Small titled row helper
    static bool BeginRow(const char* title, float titleWidth = 160.0f)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(title);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        return true;
    }

    static void DrawThemeLayoutPage()
    {
        const auto& entries = ace::ui::ThemeManager::GetThemeEntries();
        const auto  scanDir = ace::ui::ThemeManager::GetDefaultThemesDir();
        if (ImGui::BeginTable("theme_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            // --- Theme selector (auto from registry & discovery) ---
            BeginRow("Theme");
            {
                std::string currentKey = ace::ui::ThemeManager::GetActiveThemeKey();
                std::string currentName;

                int currentIndex = -1;
                for (int i = 0; i < (int)entries.size(); ++i)
                {
                    if (entries[i].key == currentKey) { currentIndex = i; currentName = entries[i].display_name; break; }
                }
                if (currentIndex == -1)
                {
                    // fallback if something changed
                    currentIndex = 0;
                    if (!entries.empty()) currentName = entries[0].display_name;
                }

                if (ImGui::BeginCombo("##theme_combo", currentName.c_str()))
                {
                    for (int i = 0; i < (int)entries.size(); ++i)
                    {
                        bool selected = (i == currentIndex);
                        if (ImGui::Selectable(entries[i].display_name.c_str(), selected))
                        {
                            ace::ui::ThemeManager::SetThemeByKey(entries[i].key);
                            g_ShowRestartModal = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::TextDisabled(" (restart recommended)");
            }

            // --- Rescan directory ---
            BeginRow("Themes Folder");
            {
                ImGui::TextUnformatted(scanDir.string().c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Rescan"))
                {
                    ace::ui::ThemeManager::DiscoverThemes(); // default dir
                }
                ImGui::SameLine();
                ImGui::TextDisabled(" Drop *.theme.json here");
            }

            // --- UI Scale (stub, live) ---
            BeginRow("UI Scale");
            {
                static float s_scale = 1.0f;
                if (ImGui::SliderFloat("##ui_scale", &s_scale, 0.75f, 1.50f, "%.2fx"))
                {
                    // Live scaling: re-apply active theme with new scale
                    ace::ui::ThemeManager::ApplyBaseTheme(s_scale);
                }
            }

            // --- Reset Dock Layout ---
            BeginRow("Layout");
            {
                if (ImGui::SmallButton("Reset to Default Layout"))
                {
                    ace::ui::ThemeManager::RebuildDefaultLayout();
                }
                ImGui::SameLine();
                ImGui::TextDisabled(" Applies current theme's default splits");
            }

            ImGui::EndTable();
        }
    }

    static void DrawGraphEditorPage()
    {
        if (ImGui::BeginTable("graph_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            BeginRow("Grid Spacing");    { static int v = 28; ImGui::SliderInt("##grid", &v, 12, 64); }
            BeginRow("Snap To Grid");    { static bool b = true; ImGui::Checkbox("##snap", &b); }
            BeginRow("Curved Links");    { static bool b = true; ImGui::Checkbox("##curves", &b); }
            BeginRow("Show Mini-Map");   { static bool b = false; ImGui::Checkbox("##minimap", &b); }
            BeginRow("Node Corner Rounding"); { static float f=4.f; ImGui::SliderFloat("##round", &f, 0.f, 10.f, "%.1f"); }

            ImGui::EndTable();
        }
    }

    static void DrawCodeEditorPage()
    {
        if (ImGui::BeginTable("code_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            BeginRow("Font Size");       { static int v=15; ImGui::SliderInt("##font", &v, 10, 22); }
            BeginRow("Show Line Numbers");{ static bool b=true; ImGui::Checkbox("##ln", &b); }
            BeginRow("Highlight Current Line");{ static bool b=true; ImGui::Checkbox("##hcline", &b); }
            BeginRow("Soft Wrap");       { static bool b=false; ImGui::Checkbox("##wrap", &b); }
            BeginRow("Indent Size");     { static int  v=4; ImGui::SliderInt("##indent", &v, 2, 8); }

            ImGui::EndTable();
        }
    }

    static void DrawGeneralPage()
    {
        if (ImGui::BeginTable("general_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            BeginRow("Language");        { static int idx=0; ImGui::Combo("##lang", &idx, "English\0Deutsch\0Français\0Nederlands\0"); }
            BeginRow("Restore Tabs on Startup"); { static bool b=true; ImGui::Checkbox("##restore", &b); }
            BeginRow("Check for Updates at Launch"); { static bool b=true; ImGui::Checkbox("##upd", &b); }

            ImGui::EndTable();
        }
    }

    static void DrawProjectsPage()
    {
        if (ImGui::BeginTable("projects_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            BeginRow("Default Project Path"); { static char buf[256] = "E:/GameProjectsNEW"; ImGui::InputText("##ppath", buf, IM_ARRAYSIZE(buf)); }
            BeginRow("Auto Open Last Project"); { static bool b=true; ImGui::Checkbox("##autoopen", &b); }

            ImGui::EndTable();
        }
    }

    static void DrawAutosavePage()
    {
        if (ImGui::BeginTable("autosave_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            BeginRow("Enable Autosave"); { static bool b=true; ImGui::Checkbox("##ena", &b); }
            BeginRow("Interval (minutes)"); { static int v=5; ImGui::SliderInt("##int", &v, 1, 30); }
            BeginRow("Backup Count");    { static int v=10; ImGui::SliderInt("##bak", &v, 1, 50); }

            ImGui::EndTable();
        }
    }

    static void DrawPluginsPage()
    {
        ImGui::TextDisabled("Plugin management UI (stub).");
        ImGui::Separator();
        ImGui::TextUnformatted("• Enable/Disable plugins\n• Install from file\n• Browse marketplace\n• Plugin load order");
    }

    static void DrawPathsPage()
    {
        if (ImGui::BeginTable("paths_tbl", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 180.f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            BeginRow("SDKs"); { static char buf[256]="C:/SDKs"; ImGui::InputText("##sdks", buf, IM_ARRAYSIZE(buf)); }
            BeginRow("Cache");{ static char buf2[256]="C:/Users/%USER%/AppData/Local/ACE/Cache"; ImGui::InputText("##cache", buf2, IM_ARRAYSIZE(buf2)); }

            ImGui::EndTable();
        }
    }

    static void DrawScriptingPage()
    {
        ImGui::TextDisabled("Custom scripting language settings (stub).");
        ImGui::Separator();
        ImGui::TextUnformatted("• Runtime path\n• Compiler path\n• Default file template\n• Lint rules & formatting");
    }

    void DrawEditorPreferences(EditorState& /*S*/)
    {
        // Hotkey: Ctrl + ,
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Comma)) g_ShowPrefs = true;

        if (!g_ShowPrefs) return;

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x*0.5f - 700*0.5f,
                                       vp->WorkPos.y + vp->WorkSize.y*0.5f - 480*0.5f),
                                ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Editor Preferences", &g_ShowPrefs, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
        {
            // Optional banner if user clicked "Later"
            if (g_ShowRestartBanner)
            {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f,0.25f,0.18f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,    ImVec4(0.85f,1.0f,0.85f,1.0f));
                ImGui::TextUnformatted("Some changes may require a restart to fully apply.");
                ImGui::SameLine();
                if (ImGui::SmallButton("Dismiss")) g_ShowRestartBanner = false;
                ImGui::PopStyleColor(2);
                ImGui::Separator();
            }

            ImGui::Columns(2, nullptr, true);
            // Left nav
            ImGui::BeginChild("prefs_nav", ImVec2(170, 0), true);
            for (int i = 0; i < (int)kSections.size(); ++i)
            {
                const bool selected = (g_SelectedSection == i);
                if (ImGui::Selectable(kSections[i].Label, selected))
                    g_SelectedSection = i;
            }
            ImGui::EndChild();

            ImGui::NextColumn();

            // Right content
            ImGui::BeginChild("prefs_body", ImVec2(0, 0), false);

            switch (g_SelectedSection)
            {
                case 0: DrawGeneralPage(); break;
                case 1: DrawThemeLayoutPage(); break;   // Theme & Layout
                case 2: DrawGraphEditorPage(); break;
                case 3: DrawCodeEditorPage(); break;
                case 4: DrawProjectsPage(); break;
                case 5: DrawAutosavePage(); break;
                case 6: DrawPluginsPage(); break;
                case 7: DrawPathsPage(); break;
                case 8: DrawScriptingPage(); break;
                default: break;
            }

            ImGui::EndChild();
            ImGui::Columns(1);

            // Footer buttons
            ImGui::Separator();
            if (ImGui::Button("Close")) g_ShowPrefs = false;
            ImGui::SameLine();
            if (ImGui::Button("Restore Defaults")) { /* TODO */ }
            ImGui::SameLine();
            if (ImGui::Button("Save"))             { /* TODO: wire to your SaveSettings(S) */ }
        }
        ImGui::End();

        // Restart recommended modal
        if (g_ShowRestartModal)
        {
            ImGui::OpenPopup("Restart Recommended");
            g_ShowRestartModal = false;
        }
        if (ImGui::BeginPopupModal("Restart Recommended", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Theme changes were applied immediately.\nA full restart is recommended to ensure all UI elements and cached resources adopt the new theme.");

            if (ImGui::Button("Later", ImVec2(120, 0)))
            {
                g_ShowRestartBanner = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}
