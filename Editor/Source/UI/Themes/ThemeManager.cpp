#include "UI/Themes/ThemeManager.h"
#include "UI/Themes/GMS2/ThemeGMS2.h"
#include "imgui.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <optional>

// nlohmann_json is already in your includes (External/nlohmann_json)
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace fs = std::filesystem;

namespace ace::ui
{
    // -------------------- internal state --------------------
    static ThemeId              g_ActiveTheme      = ThemeId::GMS2;
    static std::string          g_ActiveThemeKey   = "gms2";     // human key (built-in or json)
    static fs::path             g_ActiveJsonPath;                // for ThemeId::CustomJson
    static std::vector<ThemeManager::ThemeEntry> g_Entries;
    static ITextEditorBridge*   g_EditorBridge     = nullptr;

    // -------------------- helpers --------------------
    static fs::path DefaultThemesDir()
    {
        // Use CWD/Content/Themes by default (same base as your Fonts in main.cpp).
        return fs::current_path() / "Content" / "Themes";
    }

    fs::path ThemeManager::GetDefaultThemesDir() { return DefaultThemesDir(); }

    // Register built-ins once
    static void RegisterBuiltIns()
    {
        ThemeManager::ThemeEntry e{};
        e.key          = "gms2";
        e.display_name = "GMS2 (Dark)";
        e.is_builtin   = true;
        e.builtin_id   = ThemeId::GMS2;
        g_Entries.push_back(e);

        ThemeManager::ThemeEntry l{};
        l.key          = "gms2-light";
        l.display_name = "GMS2 (Light)";
        l.is_builtin   = true;
        l.builtin_id   = ThemeId::GMS2_Light;
        g_Entries.push_back(l);
    }

    // Parse "#RRGGBB" or "#RRGGBBAA" -> ImVec4
    static ImVec4 ParseHexRGBA(const std::string& s)
    {
        auto hex = s;
        if (!hex.empty() && hex[0] == '#') hex.erase(0, 1);
        unsigned v = 0;
        std::stringstream ss;
        ss << std::hex << hex;
        ss >> v;

        float r, g, b, a = 1.0f;
        if (hex.size() <= 6)
        {
            r = float((v >> 16) & 0xFF) / 255.f;
            g = float((v >> 8)  & 0xFF) / 255.f;
            b = float((v >> 0)  & 0xFF) / 255.f;
        }
        else
        {
            r = float((v >> 24) & 0xFF) / 255.f;
            g = float((v >> 16) & 0xFF) / 255.f;
            b = float((v >> 8)  & 0xFF) / 255.f;
            a = float((v >> 0)  & 0xFF) / 255.f;
        }
        return ImVec4(r,g,b,a);
    }

    // Map string -> ImGuiCol index (subset; extend as needed)
    static const std::unordered_map<std::string, ImGuiCol> kColorMap = {
        {"Text", ImGuiCol_Text},
        {"TextDisabled", ImGuiCol_TextDisabled},
        {"WindowBg", ImGuiCol_WindowBg},
        {"ChildBg", ImGuiCol_ChildBg},
        {"PopupBg", ImGuiCol_PopupBg},
        {"Border", ImGuiCol_Border},
        {"FrameBg", ImGuiCol_FrameBg},
        {"FrameBgHovered", ImGuiCol_FrameBgHovered},
        {"FrameBgActive", ImGuiCol_FrameBgActive},
        {"TitleBg", ImGuiCol_TitleBg},
        {"TitleBgActive", ImGuiCol_TitleBgActive},
        {"MenuBarBg", ImGuiCol_MenuBarBg},
        {"ScrollbarBg", ImGuiCol_ScrollbarBg},
        {"ScrollbarGrab", ImGuiCol_ScrollbarGrab},
        {"ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered},
        {"ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive},
        {"CheckMark", ImGuiCol_CheckMark},
        {"SliderGrab", ImGuiCol_SliderGrab},
        {"SliderGrabActive", ImGuiCol_SliderGrabActive},
        {"Button", ImGuiCol_Button},
        {"ButtonHovered", ImGuiCol_ButtonHovered},
        {"ButtonActive", ImGuiCol_ButtonActive},
        {"Header", ImGuiCol_Header},
        {"HeaderHovered", ImGuiCol_HeaderHovered},
        {"HeaderActive", ImGuiCol_HeaderActive},
        {"Separator", ImGuiCol_Separator},
        {"SeparatorHovered", ImGuiCol_SeparatorHovered},
        {"SeparatorActive", ImGuiCol_SeparatorActive},
        {"ResizeGrip", ImGuiCol_ResizeGrip},
        {"ResizeGripHovered", ImGuiCol_ResizeGripHovered},
        {"ResizeGripActive", ImGuiCol_ResizeGripActive},
        {"Tab", ImGuiCol_Tab},
        {"TabHovered", ImGuiCol_TabHovered},
        {"TabActive", ImGuiCol_TabActive},
        {"TabUnfocused", ImGuiCol_TabUnfocused},
        {"TabUnfocusedActive", ImGuiCol_TabUnfocusedActive},
        {"NavHighlight", ImGuiCol_NavHighlight},
        {"DragDropTarget", ImGuiCol_DragDropTarget},
        {"TextSelectedBg", ImGuiCol_TextSelectedBg},
        {"TableHeaderBg", ImGuiCol_TableHeaderBg},
        {"TableBorderStrong", ImGuiCol_TableBorderStrong},
        {"TableBorderLight", ImGuiCol_TableBorderLight},
    };

    static void ApplyJsonToImGui(const json& j, float scale)
    {
        ImGuiStyle& s = ImGui::GetStyle();

        // Optional scale override
        if (j.contains("scale") && j["scale"].is_number())
            s.ScaleAllSizes(j["scale"].get<float>());
        else
            s.ScaleAllSizes(scale);

        // Optional rounding & padding
        if (j.contains("rounding"))
        {
            const auto& r = j["rounding"];
            if (r.contains("Window")) s.WindowRounding = r["Window"].get<float>();
            if (r.contains("Frame"))  s.FrameRounding  = r["Frame"].get<float>();
            if (r.contains("Grab"))   s.GrabRounding   = r["Grab"].get<float>();
            if (r.contains("Tab"))    s.TabRounding    = r["Tab"].get<float>();
            if (r.contains("Popup"))  s.PopupRounding  = r["Popup"].get<float>();
        }
        if (j.contains("padding"))
        {
            const auto& p = j["padding"];
            if (p.contains("Window")) s.WindowPadding    = ImVec2(p["Window"][0], p["Window"][1]);
            if (p.contains("Frame"))  s.FramePadding     = ImVec2(p["Frame"][0],  p["Frame"][1]);
            if (p.contains("Item"))   s.ItemSpacing      = ImVec2(p["Item"][0],   p["Item"][1]);
            if (p.contains("Inner"))  s.ItemInnerSpacing = ImVec2(p["Inner"][0],  p["Inner"][1]);
        }

        // Colors
        if (j.contains("colors") && j["colors"].is_object())
        {
            ImVec4* c = s.Colors;
            for (auto it = j["colors"].begin(); it != j["colors"].end(); ++it)
            {
                const std::string name = it.key();
                auto mapIt = kColorMap.find(name);
                if (mapIt == kColorMap.end()) continue;

                if (it.value().is_string())
                {
                    c[mapIt->second] = ParseHexRGBA(it.value().get<std::string>());
                }
                else if (it.value().is_array() && it.value().size() >= 3)
                {
                    // [r,g,b,a?] 0..1
                    float r = it.value()[0].get<float>();
                    float g = it.value()[1].get<float>();
                    float b = it.value()[2].get<float>();
                    float a = (it.value().size() >= 4) ? it.value()[3].get<float>() : 1.0f;
                    c[mapIt->second] = ImVec4(r,g,b,a);
                }
            }
        }
    }

    static bool ApplyJsonThemeFile(const fs::path& path, float scale)
    {
        std::ifstream f(path);
        if (!f.is_open()) return false;

        json j; f >> j;
        ApplyJsonToImGui(j, scale);
        return true;
    }

    // -------------------- public API --------------------
    void ThemeManager::Initialize(ThemeId initial)
    {
        g_Entries.clear();
        RegisterBuiltIns();
        DiscoverThemes(); // load JSON themes under Content/Themes
        g_ActiveTheme = initial;
        g_ActiveThemeKey = (initial == ThemeId::GMS2) ? "gms2" : "custom";
    }

    void ThemeManager::SetTheme(ThemeId id)
    {
        g_ActiveTheme = id;
        if (id == ThemeId::GMS2) g_ActiveThemeKey = "gms2";
        else                     g_ActiveThemeKey = "custom";
    }

    ThemeId ThemeManager::GetActiveTheme() { return g_ActiveTheme; }

    void ThemeManager::SetTextEditorBridge(ITextEditorBridge* bridge)
    {
        g_EditorBridge = bridge;
    }

    void ThemeManager::ApplyBaseTheme(float scale)
    {
        switch (g_ActiveTheme)
        {
        case ThemeId::GMS2:
        default:
            themes::gms2::ApplyBase(scale);
            break;
            case ThemeId::GMS2_Light:
                themes::gms2::ApplyBaseLight(scale);
                break;
        case ThemeId::CustomJson:
            if (!g_ActiveJsonPath.empty())
                ApplyJsonThemeFile(g_ActiveJsonPath, scale);
            break;
        }
    }

    void ThemeManager::ApplyGraphTheme()
    {
        switch (g_ActiveTheme)
        {
        case ThemeId::GMS2:
        default:
            themes::gms2::ApplyGraphTheme();
            break;
            case ThemeId::GMS2_Light:
                themes::gms2::ApplyGraphThemeLight();
                break;
        case ThemeId::CustomJson:
            // Optional: JSON could contain graph colors in the future.
            // For now, leave default or reuse GMS2 if you prefer:
            // themes::gms2::ApplyGraphTheme();
            break;
        }
    }

    void ThemeManager::BeginMainDockspace(const char* name, bool* pOpen)
    {
        switch (g_ActiveTheme)
        {
        case ThemeId::GMS2:
        default:
            themes::gms2::BeginMainDockspace(name, pOpen);
            break;
        case ThemeId::CustomJson:
            // Use same dock host/layout for now (safe default)
            themes::gms2::BeginMainDockspace(name, pOpen);
            break;
        }
    }

    void ThemeManager::RebuildDefaultLayout()
    {
        switch (g_ActiveTheme)
        {
        case ThemeId::GMS2:
        default:
            themes::gms2::RebuildDefaultLayout();
            break;
        case ThemeId::CustomJson:
            themes::gms2::RebuildDefaultLayout();
            break;
        }
    }

    void ThemeManager::ApplyTextEditorTheme(::TextEditor& editor)
    {
        switch (g_ActiveTheme)
        {
            case ThemeId::GMS2:
            default:
                themes::gms2::ApplyTextEditorTheme(editor);
                break;
            case ThemeId::GMS2_Light:
                themes::gms2::ApplyTextEditorThemeLight(editor);
                break;
            case ThemeId::CustomJson:
                // For now: reuse GMS2 editor colors when using JSON UI themes
                themes::gms2::ApplyTextEditorTheme(editor);
                break;
        }
    }

    void ThemeManager::ThemeActiveTextEditorIfAny()
    {
        if (!g_EditorBridge) return;
        if (auto* ed = g_EditorBridge->GetActiveEditor())
        {
            ::ace::ui::ThemeManager::ApplyTextEditorTheme(*ed);
        }
    }

    void ThemeManager::DiscoverThemes(const fs::path& dir)
    {
        fs::path scanDir = dir.empty() ? DefaultThemesDir() : dir;
        if (!fs::exists(scanDir)) return;

        for (auto& p : fs::directory_iterator(scanDir))
        {
            if (!p.is_regular_file()) continue;
            if (p.path().extension() != ".json" && p.path().extension() != ".theme.json") continue;
            // Accept both *.json and *.theme.json; require presence of "colors" to be safe
            std::ifstream f(p.path());
            if (!f.is_open()) continue;

            json j;
            try { f >> j; } catch (...) { continue; }

            if (!j.contains("colors") && !j.contains("imgui")) continue; // simple sanity
            ThemeEntry e{};
            e.is_builtin   = false;
            e.builtin_id   = ThemeId::CustomJson;
            e.json_path    = p.path();

            // key/display
            if (j.contains("key") && j["key"].is_string()) e.key = j["key"].get<std::string>();
            else e.key = p.path().stem().string();

            if (j.contains("display_name") && j["display_name"].is_string())
                e.display_name = j["display_name"].get<std::string>();
            else
                e.display_name = e.key;

            // Avoid duplicates (by key)
            bool dupe = false;
            for (auto& ex : g_Entries) if (ex.key == e.key) { dupe = true; break; }
            if (!dupe) g_Entries.push_back(std::move(e));
        }
    }

    const std::vector<ThemeManager::ThemeEntry>& ThemeManager::GetThemeEntries()
    {
        return g_Entries;
    }

    bool ThemeManager::SetThemeByKey(std::string_view key, float scale)
    {
        for (auto& e : g_Entries)
        {
            if (e.key == key)
            {
                if (e.is_builtin)
                {
                    SetTheme(e.builtin_id);
                    ApplyBaseTheme(scale);
                    ApplyGraphTheme();
                    RebuildDefaultLayout();
                    g_ActiveThemeKey = std::string(key);
                    return true;
                }
                // JSON
                if (!e.json_path.empty())
                {
                    g_ActiveTheme      = ThemeId::CustomJson;
                    g_ActiveJsonPath   = e.json_path;
                    g_ActiveThemeKey   = std::string(key);
                    ApplyBaseTheme(scale);
                    ApplyGraphTheme();
                    RebuildDefaultLayout();
                    return true;
                }
            }
        }
        return false;
    }

    std::string ThemeManager::GetActiveThemeKey() { return g_ActiveThemeKey; }
}
