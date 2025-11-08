#pragma once
// ThemeManager: built-in themes + JSON-discovered themes (Content/Themes/*.theme.json)

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

// Forward declare the real editor type in the global namespace
struct TextEditor;

namespace ace::ui
{
    enum class ThemeId : int
    {
        GMS2 = 0,
        GMS2_Light = 1,
        CustomJson = 1000, // any JSON-applied theme
    };

    // The bridge should always expose the real editor type (global ::TextEditor)
    struct ITextEditorBridge
    {
        virtual ~ITextEditorBridge() = default;
        virtual ::TextEditor* GetActiveEditor() = 0; // may return nullptr
    };

    namespace ThemeManager
    {
        // One row in the theme list (either built-in or discovered JSON)
        struct ThemeEntry
        {
            std::string key;          // unique key (e.g. "gms2", "solarized-dark")
            std::string display_name; // shown in UI
            bool        is_builtin = false;
            ThemeId     builtin_id = ThemeId::GMS2;
            std::filesystem::path json_path; // for CustomJson
        };

        // --- lifecycle / config ---
        void Initialize(ThemeId initial = ThemeId::GMS2);
        void SetTheme(ThemeId id);
        ThemeId GetActiveTheme();
        void SetTextEditorBridge(ITextEditorBridge* bridge);

        // Apply base ImGui style + graph theme (no-op if missing libs)
        void ApplyBaseTheme(float scale = 1.0f);
        void ApplyGraphTheme();

        // Dock helpers
        void BeginMainDockspace(const char* name = "ACE Editor", bool* pOpen = nullptr);
        void RebuildDefaultLayout();

        // Text editor (safe no-op if not present)
        void ApplyTextEditorTheme(::TextEditor& editor);
        void ThemeActiveTextEditorIfAny();

        // --- discovery & listing ---
        // Scan 'dir' (or default Content/Themes) for *.theme.json files.
        void DiscoverThemes(const std::filesystem::path& dir = {});
        const std::vector<ThemeEntry>& GetThemeEntries();

        // Switch by key (works for either built-in or JSON-discovered).
        // Returns true on success; applies immediately.
        bool SetThemeByKey(std::string_view key, float scale = 1.0f);

        // Active theme key (for JSON themes it’s the json key; for built-ins it’s e.g. "gms2").
        std::string GetActiveThemeKey();

        // Directory ThemeManager will scan when called with default args.
        std::filesystem::path GetDefaultThemesDir();
    }
}
