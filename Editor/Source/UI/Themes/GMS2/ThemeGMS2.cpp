#include "UI/Themes/GMS2/ThemeGMS2.h"
#include "imgui.h"
#include "imgui_internal.h"

// ---- Optional libs (guarded) -------------------------------------------------
#if __has_include("imnodes.h")
    #include "imnodes.h"
    #define ACE_HAS_IMNODES 1
#else
    #define ACE_HAS_IMNODES 0
#endif

#if __has_include("TextEditor.h")
    #include "TextEditor.h"
    #define ACE_HAS_TEXTEDITOR 1
#elif __has_include("ImGuiColorTextEdit/TextEditor.h")
    #include "ImGuiColorTextEdit/TextEditor.h"
    #define ACE_HAS_TEXTEDITOR 1
#else
#define ACE_HAS_TEXTEDITOR 0
#endif
// -----------------------------------------------------------------------------

// Helper: color from 0xRRGGBB
static ImVec4 C(unsigned rgb, float a = 1.0f)
{
    const float r = ((rgb >> 16) & 0xFF) / 255.0f;
    const float g = ((rgb >>  8) & 0xFF) / 255.0f;
    const float b = ((rgb >>  0) & 0xFF) / 255.0f;
    return ImVec4(r,g,b,a);
}
static ImU32 U32(const ImVec4& v)
{
    return IM_COL32((int)(v.x*255.f), (int)(v.y*255.f), (int)(v.z*255.f), (int)(v.w*255.f));
}

// Approximate GMS2 palette: dark slates + vivid green accent.
static struct
{
    ImVec4 bg0      = C(0x1E2126); // main background
    ImVec4 bg1      = C(0x272A2F); // child/toolbars
    ImVec4 bg2      = C(0x2F3339); // frames/headers
    ImVec4 line     = C(0x3A4048); // outlines/separators
    ImVec4 text     = C(0xE6E6E6);
    ImVec4 textDim  = C(0xB8BDC3);
    ImVec4 accent   = C(0x2ED16D); // play/confirm green
    ImVec4 accentHi = C(0x3BE27C);
    ImVec4 warn     = C(0xE0A84B);
    ImVec4 error    = C(0xD96C6C);
} G;

static ImGuiID g_MainDockspace = 0;
static bool    g_DockBuilt     = false;

// ============================ Core ImGui Theme ================================
void ace::ui::themes::gms2::ApplyBase(float scale)
{
    ImGuiIO& io   = ImGui::GetIO();
    ImGuiStyle& s = ImGui::GetStyle();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    if (io.Fonts->Fonts.empty())
        io.Fonts->AddFontDefault(); // replace with your own font stack if desired

    s.ScaleAllSizes(scale);
    io.FontGlobalScale = 1.0f;

    s.WindowPadding     = ImVec2(8, 8);
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 14.0f;

    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.TabBorderSize     = 1.0f;

    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 3.0f;
    s.PopupRounding     = 4.0f;

    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = G.text;
    c[ImGuiCol_TextDisabled]          = G.textDim;
    c[ImGuiCol_WindowBg]              = G.bg0;
    c[ImGuiCol_ChildBg]               = G.bg1;
    c[ImGuiCol_PopupBg]               = C(0x15171A, 0.98f);

    c[ImGuiCol_Border]                = G.line;
    c[ImGuiCol_BorderShadow]          = C(0x000000, 0.0f);

    c[ImGuiCol_FrameBg]               = G.bg2;
    c[ImGuiCol_FrameBgHovered]        = C(0x3B414A);
    c[ImGuiCol_FrameBgActive]         = C(0x454C56);

    c[ImGuiCol_TitleBg]               = G.bg1;
    c[ImGuiCol_TitleBgActive]         = G.bg1;
    c[ImGuiCol_TitleBgCollapsed]      = G.bg1;

    c[ImGuiCol_MenuBarBg]             = G.bg1;
    c[ImGuiCol_ScrollbarBg]           = G.bg1;
    c[ImGuiCol_ScrollbarGrab]         = C(0x4B525C);
    c[ImGuiCol_ScrollbarGrabHovered]  = C(0x59616C);
    c[ImGuiCol_ScrollbarGrabActive]   = C(0x68707C);

    c[ImGuiCol_CheckMark]             = G.accent;
    c[ImGuiCol_SliderGrab]            = C(0xAEB4BD);
    c[ImGuiCol_SliderGrabActive]      = C(0xC6CBD2);

    c[ImGuiCol_Button]                = G.bg2;
    c[ImGuiCol_ButtonHovered]         = C(0x3B414A);
    c[ImGuiCol_ButtonActive]          = C(0x454C56);

    c[ImGuiCol_Header]                = C(0x353B43);
    c[ImGuiCol_HeaderHovered]         = C(0x404751);
    c[ImGuiCol_HeaderActive]          = C(0x48515C);

    c[ImGuiCol_Separator]             = G.line;
    c[ImGuiCol_SeparatorHovered]      = C(0x5A626E);
    c[ImGuiCol_SeparatorActive]       = C(0x6E7684);

    c[ImGuiCol_ResizeGrip]            = C(0x3B414A);
    c[ImGuiCol_ResizeGripHovered]     = C(0x59616C);
    c[ImGuiCol_ResizeGripActive]      = C(0x68707C);

    c[ImGuiCol_Tab]                   = C(0x2E333A);
    c[ImGuiCol_TabHovered]            = C(0x3A4048);
    c[ImGuiCol_TabActive]             = C(0x353B43);
    c[ImGuiCol_TabUnfocused]          = C(0x2A2F35);
    c[ImGuiCol_TabUnfocusedActive]    = C(0x32373E);

    c[ImGuiCol_NavHighlight]          = G.accent;
    c[ImGuiCol_DragDropTarget]        = G.accentHi;

    c[ImGuiCol_PlotLines]             = C(0x9AA3AD);
    c[ImGuiCol_PlotLinesHovered]      = C(0xC7CDD4);
    c[ImGuiCol_PlotHistogram]         = C(0x9AA3AD);
    c[ImGuiCol_PlotHistogramHovered]  = C(0xC7CDD4);

    c[ImGuiCol_TextSelectedBg]        = C(0x3A414B, 0.75f);
    c[ImGuiCol_TableHeaderBg]         = C(0x343A42);
    c[ImGuiCol_TableBorderStrong]     = G.line;
    c[ImGuiCol_TableBorderLight]      = C(0x2F343B);
}

// ============================ Dockspace + Layout ==============================
static void BuildDefaultLayout(ImGuiID dockspace)
{
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->Size);

    ImGuiID center  = dockspace;
    ImGuiID left    = 0;
    ImGuiID right   = 0;
    ImGuiID bottom  = 0;

    // GMS2-ish proportions
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.22f, &left,  &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, &right, &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,  0.26f, &bottom,&center);

    // Dock by titles (use these exact names in your panel windows)
    ImGui::DockBuilderDockWindow("Layers",          left);
    ImGui::DockBuilderDockWindow("Resources",       left);
    ImGui::DockBuilderDockWindow("Asset Browser",   right);
    ImGui::DockBuilderDockWindow("Properties",      bottom);
    ImGui::DockBuilderDockWindow("Console",         bottom);
    ImGui::DockBuilderDockWindow("Viewport",        center);
    ImGui::DockBuilderDockWindow("Room",            center);

    ImGui::DockBuilderFinish(dockspace);
}

void ace::ui::themes::gms2::BeginMainDockspace(const char* name, bool* pOpen)
{
    ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    if (ImGui::Begin(name, pOpen, hostFlags))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))   { /* ... */ ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Edit"))   { /* ... */ ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Build"))  { /* ... */ ImGui::EndMenu(); }
            if (ImGui::BeginMenu("Windows")){ /* ... */ ImGui::EndMenu(); }
            ImGui::EndMenuBar();
        }

        // Toolbar sample (Play/Stop/Build). Swap for icon font if desired.
        ImGui::PushStyleColor(ImGuiCol_Button,        G.accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, G.accentHi);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  C(0x27B35E));
        if (ImGui::Button("▶ Play")) { /* start PIE */ }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::Button("■ Stop")) { /* stop PIE */ }
        ImGui::SameLine();
        if (ImGui::Button("Build"))  { /* build action */ }

        // Dockspace
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));

        const ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        g_MainDockspace = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(g_MainDockspace, ImVec2(0, 0), dockspace_flags);

        if (!g_DockBuilt)
        {
            BuildDefaultLayout(g_MainDockspace);
            g_DockBuilt = true;
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ace::ui::themes::gms2::RebuildDefaultLayout()
{
    if (g_MainDockspace != 0)
    {
        BuildDefaultLayout(g_MainDockspace);
        g_DockBuilt = true;
    }
}

// ============================ Graph Editor (imnodes) ==========================
void ace::ui::themes::gms2::ApplyGraphTheme()
{
#if ACE_HAS_IMNODES
    using namespace imnodes;

    Style& st = GetStyle();
    st.NodeCornerRounding        = 4.0f;
    st.NodePadding               = ImVec2(8.f, 6.f);
    st.LinkThickness             = 2.0f;
    st.LinkLineSegmentsPerLength = 0.2f;
    st.GridSpacing               = 28.f;
    st.Flags |= StyleFlags_GridLinesPrimary;

    ImVec4* col = st.Colors;
    col[ImNodesCol_TitleBar]              = C(0x32373E);
    col[ImNodesCol_TitleBarHovered]       = C(0x3A4048);
    col[ImNodesCol_TitleBarSelected]      = C(0x404751);

    col[ImNodesCol_NodeBackground]        = G.bg2;
    col[ImNodesCol_NodeBackgroundHovered] = C(0x3B414A);
    col[ImNodesCol_NodeBackgroundSelected]= C(0x454C56);

    col[ImNodesCol_Link]                  = G.accent;
    col[ImNodesCol_LinkHovered]           = G.accentHi;
    col[ImNodesCol_LinkSelected]          = G.accentHi;

    col[ImNodesCol_Pin]                   = C(0xB8BDC3);
    col[ImNodesCol_PinHovered]            = G.accentHi;

    col[ImNodesCol_BoxSelector]           = C(0x3A414B, 0.25f);
    col[ImNodesCol_BoxSelectorOutline]    = G.accent;

    col[ImNodesCol_GridBackground]        = G.bg0;
    col[ImNodesCol_GridLine]              = C(0x2A2F35);
    col[ImNodesCol_GridLinePrimary]       = G.line;
#endif
}

// ============================ Code Editor (TextEditor) ========================
void ace::ui::themes::gms2::ApplyTextEditorTheme(::TextEditor& editor)
{
#if ACE_HAS_TEXTEDITOR
    using TE = ::TextEditor;
    TE::Palette p = TE::GetDarkPalette(); // start from dark, then override

    // Core surfaces
    p[(int)TE::PaletteIndex::Background]                = U32(G.bg0);
    p[(int)TE::PaletteIndex::Cursor]                    = U32(G.accent);
    p[(int)TE::PaletteIndex::Selection]                 = U32(C(0x3A414B, 0.75f));
    p[(int)TE::PaletteIndex::CurrentLineFill]           = U32(C(0x2A2F35, 0.85f));
    p[(int)TE::PaletteIndex::CurrentLineFillInactive]   = U32(C(0x272B31, 0.70f));
    p[(int)TE::PaletteIndex::LineNumber]                = U32(G.textDim);
    p[(int)TE::PaletteIndex::CurrentLineEdge]           = U32(G.line);

    // Syntax colors
    p[(int)TE::PaletteIndex::Default]                   = U32(G.text);
    p[(int)TE::PaletteIndex::Keyword]                   = U32(G.accent);
    p[(int)TE::PaletteIndex::Number]                    = U32(C(0x9AD1FF));
    p[(int)TE::PaletteIndex::String]                    = U32(C(0xE6C08B));
    p[(int)TE::PaletteIndex::CharLiteral]               = U32(C(0xE6C08B));
    p[(int)TE::PaletteIndex::Punctuation]               = U32(G.text);
    p[(int)TE::PaletteIndex::Preprocessor]              = U32(G.accentHi);
    p[(int)TE::PaletteIndex::Identifier]                = U32(C(0xDADFE6));
    p[(int)TE::PaletteIndex::KnownIdentifier]           = U32(C(0xB4E1B9));
    p[(int)TE::PaletteIndex::PreprocIdentifier]         = U32(C(0xABDDF5));
    p[(int)TE::PaletteIndex::Comment]                   = U32(C(0x9AA3AD));
    p[(int)TE::PaletteIndex::MultiLineComment]          = U32(C(0x9AA3AD));

    // Editor UI bits
    p[(int)TE::PaletteIndex::ErrorMarker]               = U32(G.error);
    p[(int)TE::PaletteIndex::Breakpoint]                = U32(G.warn);

    editor.SetPalette(p);
    editor.SetTabSize(4);
    editor.SetShowWhitespaces(false);
#endif
}

// ==== GMS2 Light palettes =====================================================
// A soft light version: gentle grays, same green accent, high-contrast text.
static struct
{
    ImVec4 bg0      = C(0xF4F6F8); // main background (very light gray)
    ImVec4 bg1      = C(0xEEF1F4); // child/toolbars
    ImVec4 bg2      = C(0xE6EAEE); // frames/headers
    ImVec4 line     = C(0xD3D9E0); // outlines/separators
    ImVec4 text     = C(0x1F2328); // near-black
    ImVec4 textDim  = C(0x5E6A75);
    ImVec4 accent   = C(0x2ED16D); // same accent
    ImVec4 accentHi = C(0x29BD62);
    ImVec4 warn     = C(0xB5730F);
    ImVec4 error    = C(0xC44B4B);
} L;

void ace::ui::themes::gms2::ApplyBaseLight(float scale)
{
    ImGuiIO& io   = ImGui::GetIO();
    ImGuiStyle& s = ImGui::GetStyle();

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    if (io.Fonts->Fonts.empty())
        io.Fonts->AddFontDefault();

    s.ScaleAllSizes(scale);
    io.FontGlobalScale = 1.0f;

    // Keep the same ergonomics as the dark theme
    s.WindowPadding     = ImVec2(8, 8);
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 14.0f;

    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 1.0f;
    s.TabBorderSize     = 1.0f;

    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 3.0f;
    s.PopupRounding     = 4.0f;

    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = L.text;
    c[ImGuiCol_TextDisabled]          = L.textDim;
    c[ImGuiCol_WindowBg]              = L.bg0;
    c[ImGuiCol_ChildBg]               = L.bg1;
    c[ImGuiCol_PopupBg]               = C(0xFFFFFF, 0.98f);

    c[ImGuiCol_Border]                = L.line;
    c[ImGuiCol_BorderShadow]          = C(0x000000, 0.0f);

    c[ImGuiCol_FrameBg]               = L.bg2;
    c[ImGuiCol_FrameBgHovered]        = C(0xDEE4EA);
    c[ImGuiCol_FrameBgActive]         = C(0xD7DEE5);

    c[ImGuiCol_TitleBg]               = L.bg1;
    c[ImGuiCol_TitleBgActive]         = L.bg1;
    c[ImGuiCol_TitleBgCollapsed]      = L.bg1;

    c[ImGuiCol_MenuBarBg]             = L.bg1;
    c[ImGuiCol_ScrollbarBg]           = L.bg1;
    c[ImGuiCol_ScrollbarGrab]         = C(0xC3CBD4);
    c[ImGuiCol_ScrollbarGrabHovered]  = C(0xB7C0CB);
    c[ImGuiCol_ScrollbarGrabActive]   = C(0xA9B4C0);

    c[ImGuiCol_CheckMark]             = L.accent;
    c[ImGuiCol_SliderGrab]            = C(0x8BA0B3);
    c[ImGuiCol_SliderGrabActive]      = C(0x6E879D);

    c[ImGuiCol_Button]                = L.bg2;
    c[ImGuiCol_ButtonHovered]         = C(0xDEE4EA);
    c[ImGuiCol_ButtonActive]          = C(0xD7DEE5);

    c[ImGuiCol_Header]                = C(0xDEE4EA);
    c[ImGuiCol_HeaderHovered]         = C(0xD7DEE5);
    c[ImGuiCol_HeaderActive]          = C(0xCFD7DE);

    c[ImGuiCol_Separator]             = L.line;
    c[ImGuiCol_SeparatorHovered]      = C(0xA9B4C0);
    c[ImGuiCol_SeparatorActive]       = C(0x93A2B0);

    c[ImGuiCol_ResizeGrip]            = C(0xC3CBD4);
    c[ImGuiCol_ResizeGripHovered]     = C(0xB7C0CB);
    c[ImGuiCol_ResizeGripActive]      = C(0xA9B4C0);

    c[ImGuiCol_Tab]                   = C(0xE8ECF0);
    c[ImGuiCol_TabHovered]            = C(0xDEE4EA);
    c[ImGuiCol_TabActive]             = C(0xD7DEE5);
    c[ImGuiCol_TabUnfocused]          = C(0xECEFF3);
    c[ImGuiCol_TabUnfocusedActive]    = C(0xE3E8ED);

    c[ImGuiCol_NavHighlight]          = L.accent;
    c[ImGuiCol_DragDropTarget]        = L.accentHi;

    c[ImGuiCol_PlotLines]             = C(0x6E879D);
    c[ImGuiCol_PlotLinesHovered]      = C(0x4D6A84);
    c[ImGuiCol_PlotHistogram]         = C(0x6E879D);
    c[ImGuiCol_PlotHistogramHovered]  = C(0x4D6A84);

    c[ImGuiCol_TextSelectedBg]        = C(0xBFD7C8, 0.75f);
    c[ImGuiCol_TableHeaderBg]         = C(0xE6EAEE);
    c[ImGuiCol_TableBorderStrong]     = L.line;
    c[ImGuiCol_TableBorderLight]      = C(0xE9EDF2);
}

void ace::ui::themes::gms2::ApplyGraphThemeLight()
{
#if ACE_HAS_IMNODES
    using namespace imnodes;
    Style& st = GetStyle();
    st.NodeCornerRounding        = 4.0f;
    st.NodePadding               = ImVec2(8.f, 6.f);
    st.LinkThickness             = 2.0f;
    st.LinkLineSegmentsPerLength = 0.2f;
    st.GridSpacing               = 28.f;
    st.Flags |= StyleFlags_GridLinesPrimary;

    ImVec4* col = st.Colors;
    col[ImNodesCol_TitleBar]              = C(0xE6EAEE);
    col[ImNodesCol_TitleBarHovered]       = C(0xDEE4EA);
    col[ImNodesCol_TitleBarSelected]      = C(0xD7DEE5);

    col[ImNodesCol_NodeBackground]        = L.bg2;
    col[ImNodesCol_NodeBackgroundHovered] = C(0xDEE4EA);
    col[ImNodesCol_NodeBackgroundSelected]= C(0xD7DEE5);

    col[ImNodesCol_Link]                  = L.accent;
    col[ImNodesCol_LinkHovered]           = L.accentHi;
    col[ImNodesCol_LinkSelected]          = L.accentHi;

    col[ImNodesCol_Pin]                   = C(0x6E879D);
    col[ImNodesCol_PinHovered]            = L.accentHi;

    col[ImNodesCol_BoxSelector]           = C(0xBFD7C8, 0.25f);
    col[ImNodesCol_BoxSelectorOutline]    = L.accent;

    col[ImNodesCol_GridBackground]        = L.bg0;
    col[ImNodesCol_GridLine]              = C(0xE3E8ED);
    col[ImNodesCol_GridLinePrimary]       = L.line;
#endif
}

void ace::ui::themes::gms2::ApplyTextEditorThemeLight(::TextEditor& editor)
{
#if ACE_HAS_TEXTEDITOR
    using TE = ::TextEditor;
    TE::Palette p = TE::GetLightPalette(); // base light
    // Nudge a few colors to match our light UI + green accent
    auto toU32 = [](const ImVec4& v)->ImU32 { return IM_COL32((int)(v.x*255.f),(int)(v.y*255.f),(int)(v.z*255.f),(int)(v.w*255.f)); };

    p[(int)TE::PaletteIndex::Background]              = toU32(L.bg0);
    p[(int)TE::PaletteIndex::Cursor]                  = toU32(L.accent);
    p[(int)TE::PaletteIndex::Selection]               = toU32(C(0xBFD7C8, 0.65f));
    p[(int)TE::PaletteIndex::CurrentLineFill]         = toU32(C(0xEEF2F6, 1.0f));
    p[(int)TE::PaletteIndex::CurrentLineFillInactive] = toU32(C(0xF2F5F8, 1.0f));
    p[(int)TE::PaletteIndex::LineNumber]              = toU32(L.textDim);
    p[(int)TE::PaletteIndex::CurrentLineEdge]         = toU32(L.line);

    // Keep syntax readable on light background
    p[(int)TE::PaletteIndex::Default]                 = toU32(L.text);
    p[(int)TE::PaletteIndex::Keyword]                 = toU32(C(0x0F7D3E)); // darker green for keywords on light bg
    p[(int)TE::PaletteIndex::Number]                  = toU32(C(0x1F6FEB)); // readable blue
    p[(int)TE::PaletteIndex::String]                  = toU32(C(0xA15600)); // brownish strings
    p[(int)TE::PaletteIndex::CharLiteral]             = p[(int)TE::PaletteIndex::String];
    p[(int)TE::PaletteIndex::Punctuation]             = toU32(L.text);
    p[(int)TE::PaletteIndex::Preprocessor]            = toU32(C(0x7A3E9D)); // violet
    p[(int)TE::PaletteIndex::Identifier]              = toU32(C(0x2D333A));
    p[(int)TE::PaletteIndex::KnownIdentifier]         = toU32(C(0x0F7D3E));
    p[(int)TE::PaletteIndex::PreprocIdentifier]       = toU32(C(0x005CC5));
    p[(int)TE::PaletteIndex::Comment]                 = toU32(C(0x7B8A97));
    p[(int)TE::PaletteIndex::MultiLineComment]        = toU32(C(0x7B8A97));

    p[(int)TE::PaletteIndex::ErrorMarker]             = toU32(L.error);
    p[(int)TE::PaletteIndex::Breakpoint]              = toU32(L.warn);

    editor.SetPalette(p);
    editor.SetTabSize(4);
    editor.SetShowWhitespaces(false);
#endif
}
