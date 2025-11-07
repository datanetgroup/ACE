// Main Patch
// ACE Editor – ImGui + GLFW + OpenGL2
// Content Browser + Asset creation + text editor + simple Blueprint node canvas.
//
// Requires Dear ImGui "docking" branch (ACE_USE_IMGUI_DOCKING=1).
// Multi-viewport intentionally disabled so panels remain inside the main window.

#include <cstdio>
#include <optional>
#include <filesystem>
#include <string>
#include <fstream>
#include <exception>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <set>
#include <cstdarg>
#include <ctime>
#include <thread>
#include "EditorSettingsPanel.h"
#include "EditorCodegen.h"

#include <GLFW/glfw3.h>
#ifdef _WIN32
  #include <Windows.h>
  #include <commdlg.h>    // GetOpenFileNameW
  #include <shobjidl.h>   // IFileDialog (for folder picker)
  #include <shellapi.h>   // ShellExecuteW
#endif
#include <GL/gl.h>

// Enable ImVec2 operators like +, -, * before including imgui.h
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"
// std::string helpers for InputText*
#include "misc/cpp/imgui_stdlib.h"

#ifndef IMGUI_HAS_DOCK
#error "Dear ImGui was built without docking. Use the docking branch."
#endif

#include "Runtime/Project/Project.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#ifndef ACE_USE_IMGUI_DOCKING
#define ACE_USE_IMGUI_DOCKING 1
#endif

#ifndef ACE_SOURCE_DIR
#define ACE_SOURCE_DIR "."
#endif

// ---------- Helpers ----------

static std::string CanonicalStr(const std::filesystem::path& p) {
    std::error_code ec;
    auto w = std::filesystem::weakly_canonical(p, ec);
    return (ec ? p.lexically_normal() : w).string();
}

static bool PathsEqual(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    if (std::filesystem::equivalent(a, b, ec)) return true;
    return CanonicalStr(a) == CanonicalStr(b);
}

static std::string ToLower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

enum class EBuildConfig   { Debug, Development, Shipping };
enum class EBuildTarget   { Game, Editor, Server };
enum class EBuildPlatform { Windows, Linux, Mac };

// Build control (forward decls; definitions appear later in the file)
static void StartBuild(EditorState& S, bool bRebuild, bool bClean);
static void CancelBuild(EditorState& S);

// --- string conversions ---
static inline const char* ToStr(EBuildConfig v) {
    switch (v) {
        case EBuildConfig::Debug:        return "Debug";
        case EBuildConfig::Development:  return "Development";
        case EBuildConfig::Shipping:     return "Shipping";
    }
    return "?";
}
static inline const char* ToStr(EBuildTarget v) {
    switch (v) {
        case EBuildTarget::Game:   return "Game";
        case EBuildTarget::Editor: return "Editor";
        case EBuildTarget::Server: return "Server";
    }
    return "?";
}
static inline const char* ToStr(EBuildPlatform v) {
    switch (v) {
        case EBuildPlatform::Windows: return "Windows";
        case EBuildPlatform::Linux:   return "Linux";
        case EBuildPlatform::Mac:     return "macOS";
    }
    return "?";
}

static inline bool FromStr(const std::string& s, EBuildConfig& out) {
    const auto t = ToLower(s);
    if (t == "debug")                        { out = EBuildConfig::Debug;        return true; }
    if (t == "development" || t == "dev")    { out = EBuildConfig::Development;  return true; }
    if (t == "shipping"    || t == "release"){ out = EBuildConfig::Shipping;     return true; }
    return false;
}

static inline bool FromStr(const std::string& s, EBuildTarget& out) {
    const auto t = ToLower(s);
    if (t == "game")   { out = EBuildTarget::Game;   return true; }
    if (t == "editor") { out = EBuildTarget::Editor; return true; }
    if (t == "server") { out = EBuildTarget::Server; return true; }
    return false;
}

static inline bool FromStr(const std::string& s, EBuildPlatform& out) {
    const auto t = ToLower(s);
    if (t == "windows" || t == "win") { out = EBuildPlatform::Windows; return true; }
    if (t == "linux")                 { out = EBuildPlatform::Linux;   return true; }
    if (t == "mac" || t == "macos")   { out = EBuildPlatform::Mac;     return true; }
    return false;
}

struct EditorState;
static void StartBuild(EditorState& S, bool bRebuild, bool bClean);
static void CancelBuild(EditorState& S);


// --- build selection/runtime ---
struct BuildSelection {
    EBuildConfig   Config   = EBuildConfig::Development;
    EBuildTarget   Target   = EBuildTarget::Game;
#if defined(_WIN32)
    EBuildPlatform Platform = EBuildPlatform::Windows;
#elif defined(__APPLE__)
    EBuildPlatform Platform = EBuildPlatform::Mac;
#else
    EBuildPlatform Platform = EBuildPlatform::Linux;
#endif
    bool        bOpenOutputOnSuccess = true;
    int         ParallelJobs = (int)std::max(1u, std::thread::hardware_concurrency());
    std::string ExtraArgs; // free-form CLI args for your build tool
};

struct BuildRuntime {
    bool        IsRunning       = false;
    bool        CancelRequested = false;
    float       Progress        = 0.0f;      // 0..1
    std::string Step;                        // "Preparing", "Generating", "Compiling", ...
};

// ---------- Editor State ----------

struct Panels {
    bool Viewport         = true;
    bool WorldOutliner    = true;
    bool Inspector        = true;
    bool ContentBrowser   = true;
    bool Console          = true;
    bool Profiler         = false;
    bool BuildOutput      = false;
    bool PlayControls     = true;
    bool Editors          = true;    // Text/Blueprint editors
    bool Settings_Input   = false;
    bool Settings_Rendering  = false;
    bool Settings_Physics = true;
    bool Settings_Audio   = false;
    bool Settings_Scripting = false;
    bool Settings_Networking = false;
    bool Settings_Online = false;
    bool Settings_AssetManager = false;
    bool Settings_Gameplay = false;
    bool Settings_ContentPaths = false;
    bool Settings_Localization = false;
    bool Settings_Build = false;
    bool Settings_Platforms = false;
    bool Settings_SourceControl = false;
    bool Settings_Appearance = false;
    bool Settings_Keymap = false;
    bool Settings_Diagnostics = false;
    bool Settings_Memory = false;
    bool Settings_CVars = false;

    bool ProjectSettings = false;
};

struct ItemRect {
    std::filesystem::path path;
    ImVec2 min{}, max{};
    bool isDir = false;
};

struct ContentBrowserState {
    std::filesystem::path Root;         // absolute Content/
    std::filesystem::path Current;      // absolute

    // Selection
    std::vector<std::filesystem::path> Selection; // multi
    int AnchorIndex = -1;                          // for Shift range

    // Clipboard (copy)
    std::vector<std::filesystem::path> Clipboard;

    // Grid view
    float ThumbnailSize = 96.0f;        // square icon size
    float Padding       = 12.0f;        // item padding
    std::string Filter;                 // substring filter (case-sensitive for now)

    // Marquee selection
    bool DragSelecting = false;
    ImVec2 DragStart{}, DragCur{};
    std::vector<ItemRect> ItemRects;    // per-frame, for marquee hit-test

    // Creation / rename / delete popups
    bool ShowNewFolder       = false;
    bool ShowNewFileTxt      = false;
    bool ShowNewFileCustom   = false;
    bool ShowRename          = false;
    bool ShowDeleteConfirm   = false;      // single or multi
    std::vector<std::filesystem::path> DeleteList;

    char NameBuf[256]   {};   // used for new folder / new file
    char ExtBuf[64]     {};   // used for custom extension
    char RenameBuf[256] {};   // used for rename

    std::filesystem::path TargetPath;   // for rename
    std::string Error;                  // last error

    // Sorting (future: expose UI)
    enum class SortMode { NameAsc } Sort = SortMode::NameAsc;

    // Folder tree
    float TreeWidth = 260.0f; // left sidebar width
    bool  TreeResizing = false;

    // Persisted
    std::filesystem::path LastFolder;

    // --- New Item dialog state (for templates like Blueprint/GameMode/Data Asset) ---
    bool ShowNewItemDialog = false;
    char NewItemNameBuf[256] {};
    char NewItemTemplateId[64] {};
    std::filesystem::path NewItemTargetFolder;
    std::string NewItemError;
};


// --- Simple tabbed editors ---

enum class EditorTabType { Text, Blueprint };

static void Logf(const char* fmt, ...);

namespace bp {

// ----- Minimal Blueprint Data -----

enum class PinKind { Input, Output };
enum class ValueType { Float }; // extend later

struct Pin {
    int id = 0;
    std::string name;
    PinKind kind = PinKind::Input;
    ValueType type = ValueType::Float;
};

struct Node {
    int id = 0;
    std::string title;
    ImVec2 pos = ImVec2(100,100);
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
};

struct Link {
    int id = 0;
    int fromNode = 0, fromPin = 0;
    int toNode   = 0, toPin   = 0;
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Link> links;
    int nextId = 1;

    int NewId() { return nextId++; }
    Node* FindNode(int nid) {
        for (auto& n : nodes) if (n.id==nid) return &n; return nullptr;
    }
    const Node* FindNode(int nid) const {
        for (auto& n : nodes) if (n.id==nid) return &n; return nullptr;
    }
    Pin*  FindPin(int nid, int pid) {
        Node* n = FindNode(nid); if (!n) return nullptr;
        for (auto& p : n->inputs)  if (p.id==pid) return &p;
        for (auto& p : n->outputs) if (p.id==pid) return &p;
        return nullptr;
    }
    const Pin*  FindPin(int nid, int pid) const {
        const Node* n = FindNode(nid); if (!n) return nullptr;
        for (auto& p : n->inputs)  if (p.id==pid) return &p;
        for (auto& p : n->outputs) if (p.id==pid) return &p;
        return nullptr;
    }
};


// UI state for the canvas (per opened asset)
struct UI {
    ImVec2 pan = ImVec2(0,0);
    float  zoom = 1.0f;               // kept for later, not used in math
    int    selectedNode = 0;

    // Linking drag
    bool   linking = false;
    int    linkFromNode = 0;
    int    linkFromPin  = 0;
    ImVec2 linkFromPos  = ImVec2(0,0);
};

} // namespace bp


struct EditorTab {
    EditorTabType Type = EditorTabType::Text;

    // Common
    std::filesystem::path Path;
    std::string Title;

    // Text
    std::string Buffer;
    bool Dirty = false;
    bool ReadOnly = false;

    // Blueprint
    bp::Graph BPGraph;
    bp::UI    BpUI;
    bool      BPLoaded = false;
    bool      BPDirty  = false;
};

// ---------- Editor World / Map types (minimal) ----------

struct EVec3 { float x=0, y=0, z=0; };
static inline EVec3 EV3(float x=0, float y=0, float z=0){ return EVec3{x,y,z}; }

struct ETransform {
    EVec3 position = EV3(0,0,0);
    EVec3 rotation = EV3(0,0,0);
    EVec3 scale    = EV3(1,1,1);
};

enum class EComponentType { StaticMesh };

struct EStaticMeshComponent {
    std::filesystem::path Mesh;     // e.g. /Game/Props/SM_Crate.aceasset
    std::filesystem::path Material; // optional
};

struct EComponent {
    EComponentType Type = EComponentType::StaticMesh;
    // Only one concrete type for now:
    EStaticMeshComponent StaticMesh;
};

struct EEntity {
    int                Id = 0;
    std::string        Name = "Entity";
    ETransform         Xf;
    std::vector<EComponent> Components;
    // (Future: Children / Parent for hierarchy)
};

struct EWorld {
    std::vector<EEntity> Entities;
    int NextId = 1;
};

// JSON helpers
static inline json ToJson(const EVec3& v){ return json::array({v.x, v.y, v.z}); }
static inline EVec3 FromJsonVec3(const json& j){
    EVec3 v{};
    if (j.is_array() && j.size()>=3){ v.x=float(j[0]); v.y=float(j[1]); v.z=float(j[2]); }
    return v;
}
static inline json ToJson(const ETransform& t){
    return json{ {"Pos", ToJson(t.position)}, {"Rot", ToJson(t.rotation)}, {"Scale", ToJson(t.scale)} };
}
static inline ETransform FromJsonTransform(const json& j){
    ETransform t{};
    if (j.contains("Pos"))   t.position = FromJsonVec3(j["Pos"]);
    if (j.contains("Rot"))   t.rotation = FromJsonVec3(j["Rot"]);
    if (j.contains("Scale")) t.scale    = FromJsonVec3(j["Scale"]);
    return t;
}


struct EditorState {
    std::optional<ace::Project> Project;
    std::filesystem::path       ProjectFile;

    bool ShowDemo = false;

    // Project Settings modal
    bool        ShowProjectSettings = false;
    std::string NameBuf;
    std::string VersionBuf;
    bool        SettingsDirty = false;

    // Recent projects
    std::vector<std::filesystem::path> Recent;

    // Panels
    Panels P;

    // Content Browser
    ContentBrowserState CB;

    // Editors (text / blueprint)
    std::vector<EditorTab> Tabs;
    int  ActiveTab = -1;
    bool FocusNewTab = false;

    // Layout
    bool ResetLayoutRequested = false;

    BuildSelection BuildSel;
    BuildRuntime   Build;

    // -------- NEW: Map / World authoring --------
    std::filesystem::path OpenMapPath;  // absolute path to .acemap
    bool                  MapDirty = false;
    EWorld                EditorWorld;  // in-editor world data
    int                   SelectedEntity = -1; // index into EditorWorld.Entities, or -1 if none
};



// ---------- Utilities ----------

// --- Forward decls for .acemap dialogs (defined later near Win32 helpers) ---
static std::optional<std::filesystem::path> OpenAcemapDialog();
static std::optional<std::filesystem::path> SaveAcemapDialog(
    const std::filesystem::path& suggestFolder, const char* suggestName);


    // ---------- Map I/O & World ops ----------

static void ClearWorld(EditorState& S){
    S.EditorWorld = EWorld{};
    S.SelectedEntity = -1;
    S.MapDirty = false;
}

static int WorldAddEntity(EWorld& W, const std::string& name){
    EEntity e; e.Id = W.NextId++; e.Name = name;
    W.Entities.push_back(std::move(e));
    return (int)W.Entities.size()-1;
}

static json ToJson(const EComponent& c){
    json j;
    switch (c.Type){
        case EComponentType::StaticMesh:
            j["Type"] = "StaticMesh";
            j["Mesh"] = c.StaticMesh.Mesh.generic_string();
            if (!c.StaticMesh.Material.empty())
                j["Material"] = c.StaticMesh.Material.generic_string();
            break;
    }
    return j;
}
static EComponent FromJsonComponent(const json& j){
    EComponent c;
    std::string t = j.value("Type", "StaticMesh");
    if (t == "StaticMesh"){
        c.Type = EComponentType::StaticMesh;
        if (j.contains("Mesh"))     c.StaticMesh.Mesh     = j["Mesh"].get<std::string>();
        if (j.contains("Material")) c.StaticMesh.Material = j["Material"].get<std::string>();
    }
    return c;
}

static json ToJson(const EEntity& e){
    json j;
    j["Id"] = e.Id;
    j["Name"] = e.Name;
    j["Transform"] = ToJson(e.Xf);
    j["Components"] = json::array();
    for (auto& c : e.Components) j["Components"].push_back(ToJson(c));
    return j;
}
static EEntity FromJsonEntity(const json& j){
    EEntity e;
    e.Id   = j.value("Id", 0);
    e.Name = j.value("Name", std::string("Entity"));
    if (j.contains("Transform")) e.Xf = FromJsonTransform(j["Transform"]);
    if (j.contains("Components")){
        for (auto& jc : j["Components"]) e.Components.push_back(FromJsonComponent(jc));
    }
    return e;
}

static bool SaveMapToFile(const EditorState& S, const std::filesystem::path& path){
    json root;
    root["Type"] = "Map";
    root["Version"] = 1;
    root["Entities"] = json::array();
    for (auto& e : S.EditorWorld.Entities) root["Entities"].push_back(ToJson(e));
    std::error_code ec; std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << root.dump(2);
    return true;
}

static bool LoadMapFromFile(EditorState& S, const std::filesystem::path& path){
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    json j; in >> j;
    EWorld W{};
    auto body = j.contains("Entities") ? j["Entities"] : json::array();
    int maxId = 0;
    for (auto& je : body){
        auto e = FromJsonEntity(je);
        maxId = std::max(maxId, e.Id);
        W.Entities.push_back(std::move(e));
    }
    W.NextId = std::max(1, maxId+1);
    S.EditorWorld = std::move(W);
    S.SelectedEntity = S.EditorWorld.Entities.empty() ? -1 : 0;
    S.OpenMapPath = path;
    S.MapDirty = false;
    return true;
}

// High-level editor ops
static void NewEmptyMap(EditorState& S){
    ClearWorld(S);
    // Seed with one default entity so there is something to select
    int idx = WorldAddEntity(S.EditorWorld, "EmptyActor");
    S.EditorWorld.Entities[idx].Xf.position = EV3(0,0,0);
    S.OpenMapPath.clear();
    S.MapDirty = true;
}

static bool SaveMap(EditorState& S){
    if (S.OpenMapPath.empty()){
        auto p = SaveAcemapDialog(S.Project ? S.Project->ContentDir() : std::filesystem::path{}, "NewMap.acemap");
        if (!p) return false;
        if (!SaveMapToFile(S, *p)) return false;
        S.OpenMapPath = *p;
        S.MapDirty = false;
        return true;
    } else {
        if (!SaveMapToFile(S, S.OpenMapPath)) return false;
        S.MapDirty = false;
        return true;
    }
}

static bool SaveMapAs(EditorState& S){
    auto suggest = S.OpenMapPath.empty() ? (S.Project ? S.Project->ContentDir() : std::filesystem::path{}) : S.OpenMapPath.parent_path();
    auto name = S.OpenMapPath.empty() ? "NewMap.acemap" : S.OpenMapPath.filename().string().c_str();
    auto p = SaveAcemapDialog(suggest, name);
    if (!p) return false;
    if (!SaveMapToFile(S, *p)) return false;
    S.OpenMapPath = *p;
    S.MapDirty = false;
    return true;
}

static bool OpenMap(EditorState& S){
    auto sel = OpenAcemapDialog();
    if (!sel) return false;
    return LoadMapFromFile(S, *sel);
}


static std::optional<std::filesystem::path> ParseProjectArg(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--project")
            return std::filesystem::path(argv[i + 1]);
    return std::nullopt;
}

#ifdef _WIN32
static std::filesystem::path GetExeDir() {
    wchar_t buf[MAX_PATH]{0};
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0)
        return std::filesystem::path(buf).parent_path();
    return std::filesystem::current_path();
}
#else
static std::filesystem::path GetExeDir() { return std::filesystem::current_path(); }
#endif

static std::filesystem::path TemplatesDir() {
    const auto exeDir = GetExeDir();
    const auto local = exeDir / "Templates";
    if (std::filesystem::exists(local)) { std::printf("[ACEEditor] Templates: %s\n", local.string().c_str()); return local; }
    const auto fromMacro = std::filesystem::path(ACE_SOURCE_DIR) / "Templates";
    if (std::filesystem::exists(fromMacro)) { std::printf("[ACEEditor] Templates: %s\n", fromMacro.string().c_str()); return fromMacro; }
    const auto probe = exeDir.parent_path().parent_path().parent_path() / "Templates";
    std::printf("[ACEEditor] Templates (probe): %s\n", probe.string().c_str());
    return probe;
}

#ifdef _WIN32
static std::optional<std::filesystem::path> OpenAceprojDialog() {
    wchar_t fileBuf[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"ACE Project (*.aceproj)\0*.aceproj\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"aceproj";
    if (GetOpenFileNameW(&ofn)) return std::filesystem::path(fileBuf);
    return std::nullopt;
}
static std::optional<std::filesystem::path> PickFolderDialog() {
    std::optional<std::filesystem::path> result;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileDialog* pfd = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
        if (SUCCEEDED(hr)) {
            DWORD opts; pfd->GetOptions(&opts); pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            if (SUCCEEDED(pfd->Show(nullptr))) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(pfd->GetResult(&psi))) {
                    PWSTR pszPath = nullptr;
                    if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                        result = std::filesystem::path(pszPath);
                        CoTaskMemFree(pszPath);
                    }
                    psi->Release();
                }
            }
            pfd->Release();
        }
        CoUninitialize();
    }
    return result;
}
static void RevealInExplorer(const std::filesystem::path& p) {
    if (std::filesystem::exists(p) && std::filesystem::is_regular_file(p)) {
        std::wstring cmd = L"/select,\"" + p.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(nullptr, L"open", L"explorer.exe", p.wstring().c_str(), nullptr, SW_SHOWNORMAL);
    }
}
#else
static std::optional<std::filesystem::path> OpenAceprojDialog() { return std::nullopt; }
static std::optional<std::filesystem::path> PickFolderDialog()  { return std::nullopt; }
static void RevealInExplorer(const std::filesystem::path&) {}
#endif

static bool IsMapFile(const std::filesystem::path& p){
    return ToLower(p.extension().string()) == ".acemap";
}

#ifdef _WIN32
static std::optional<std::filesystem::path> OpenAcemapDialog() {
    wchar_t fileBuf[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFilter = L"ACE Map (*.acemap)\0*.acemap\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = fileBuf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrDefExt = L"acemap";
    if (GetOpenFileNameW(&ofn)) return std::filesystem::path(fileBuf);
    return std::nullopt;
}
static std::optional<std::filesystem::path> SaveAcemapDialog(const std::filesystem::path& suggestFolder, const char* suggestName) {
    wchar_t fileBuf[MAX_PATH] = L"";
    std::wstring dir = suggestFolder.empty() ? L"" : suggestFolder.wstring();
    std::wstring name = suggestName ? std::wstring(suggestName, suggestName + std::strlen(suggestName)) : L"NewMap.acemap";
    std::wstring initial = dir;
    if (!initial.empty()) { if (initial.back() != L'\\' && initial.back() != L'/') initial.push_back(L'\\'); }
    initial += name;
    std::wcsncpy(fileBuf, initial.c_str(), MAX_PATH-1);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFilter = L"ACE Map (*.acemap)\0*.acemap\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = fileBuf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    ofn.lpstrDefExt = L"acemap";
    if (GetSaveFileNameW(&ofn)) {
        std::filesystem::path p(fileBuf);
        if (ToLower(p.extension().string()) != ".acemap") p.replace_extension(".acemap");
        return p;
    }
    return std::nullopt;
}
#else
static std::optional<std::filesystem::path> OpenAcemapDialog() { return std::nullopt; }
static std::optional<std::filesystem::path> SaveAcemapDialog(const std::filesystem::path&, const char*) { return std::nullopt; }
#endif


static bool CopyDirectoryContents(const std::filesystem::path& src, const std::filesystem::path& dst) {
    if (!std::filesystem::exists(src) || !std::filesystem::is_directory(src)) return false;
    std::error_code ec;
    std::filesystem::create_directories(dst, ec);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(src)) {
        const auto rel  = std::filesystem::relative(entry.path(), src, ec); if (ec) continue;
        const auto dest = dst / rel;
        if (entry.is_directory()) {
            std::filesystem::create_directories(dest, ec);
        } else if (entry.is_regular_file()) {
            std::filesystem::create_directories(dest.parent_path(), ec);
            std::filesystem::copy_file(entry.path(), dest, std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
    return true;
}

static std::filesystem::path MakeUniqueChildDirectory(const std::filesystem::path& parent, const std::string& baseName) {
    std::filesystem::path candidate = parent / baseName;
    if (!std::filesystem::exists(candidate)) return candidate;
    for (int i = 1; i < 1000; ++i) {
        std::filesystem::path alt = parent / (baseName + " (" + std::to_string(i) + ")");
        if (!std::filesystem::exists(alt)) return alt;
    }
    return parent / (baseName + " (copy)");
}

// Unique sibling filename (keeps extension): "Name.ext" -> "Name - Copy.ext" / "Name - Copy (2).ext"
static std::filesystem::path UniqueSibling(const std::filesystem::path& parent, const std::string& base) {
    std::filesystem::path candidate = parent / base;
    if (!std::filesystem::exists(candidate)) return candidate;
    // Split name/ext
    std::filesystem::path stem = std::filesystem::path(base).stem();
    std::filesystem::path ext  = std::filesystem::path(base).extension();
    auto name = stem.string();
    auto extension = ext.string();
    // Try "Name - Copy.ext", then (2..999)
    std::string first = name + " - Copy" + extension;
    if (!std::filesystem::exists(parent / first)) return parent / first;
    for (int i=2;i<1000;++i) {
        std::string variant = name + " - Copy (" + std::to_string(i) + ")" + extension;
        if (!std::filesystem::exists(parent / variant)) return parent / variant;
    }
    return parent / (name + " - Copy (999)" + extension);
}

// Robust "move into folder" with copy fallback (handles cross-volume moves, in-use files, etc.)
static bool MoveEntryToDir(const std::filesystem::path& src,
                           const std::filesystem::path& dstDir,
                           std::string& outErr)
{
    outErr.clear();
    Logf("MoveEntryToDir: src='%s' -> dstDir='%s'",
         src.string().c_str(), dstDir.string().c_str());

    std::error_code ec;
    if (!std::filesystem::exists(src)) {
        outErr = "Source does not exist.";
        Logf("MoveEntryToDir: FAIL (src missing)");
        return false;
    }

    if (!std::filesystem::exists(dstDir)) {
        std::filesystem::create_directories(dstDir, ec);
        if (ec) { outErr = "Failed to create destination directory.";
                  Logf("MoveEntryToDir: FAIL create dstDir: %s", ec.message().c_str());
                  return false; }
    }

    const auto dest = UniqueSibling(dstDir, src.filename().string());
    Logf("MoveEntryToDir: UniqueSibling -> '%s'", dest.string().c_str());

    // Try fast rename
    ec.clear();
    std::filesystem::rename(src, dest, ec);
    if (!ec) {
        Logf("MoveEntryToDir: rename OK");
        return true;
    }
    Logf("MoveEntryToDir: rename failed -> %s. Falling back to copy+delete.", ec.message().c_str());

    if (std::filesystem::is_directory(src)) {
        // copy tree
        ec.clear();
        std::filesystem::create_directories(dest, ec);
        if (ec) { outErr = "Failed to create destination."; Logf("MoveEntryToDir: FAIL create dest: %s", ec.message().c_str()); return false; }

        std::error_code it_ec;
        for (std::filesystem::recursive_directory_iterator it(src, it_ec), end; !it_ec && it != end; ++it) {
            const auto& e = *it;
            std::error_code rel_ec;
            auto rel = std::filesystem::relative(e.path(), src, rel_ec);
            if (rel_ec) { outErr = "Relpath failed during copy."; Logf("MoveEntryToDir: FAIL relpath: %s", rel_ec.message().c_str()); return false; }
            auto d = dest / rel;

            if (e.is_directory()) {
                std::filesystem::create_directories(d, ec);
                if (ec) { outErr = "Failed to make subdir during copy."; Logf("MoveEntryToDir: FAIL mkdir subdir: %s", ec.message().c_str()); return false; }
            } else if (e.is_regular_file()) {
                std::filesystem::create_directories(d.parent_path(), ec);
                if (ec) { outErr = "Failed to create parent for file copy."; Logf("MoveEntryToDir: FAIL create parent: %s", ec.message().c_str()); return false; }
                ec.clear();
                std::filesystem::copy_file(e.path(), d, std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) { outErr = "Failed to copy file."; Logf("MoveEntryToDir: FAIL copy file '%s' -> '%s': %s",
                                                                e.path().string().c_str(), d.string().c_str(), ec.message().c_str()); return false; }
            }
        }
        ec.clear();
        std::filesystem::remove_all(src, ec);
        if (ec) { outErr = "Copied, but failed to delete source directory."; Logf("MoveEntryToDir: FAIL remove_all src: %s", ec.message().c_str()); return false; }
        Logf("MoveEntryToDir: directory copy+delete OK");
        return true;
    } else {
        // copy single file
        std::filesystem::create_directories(dest.parent_path(), ec);
        if (ec) { outErr = "Failed to create destination parent directory."; Logf("MoveEntryToDir: FAIL create parent: %s", ec.message().c_str()); return false; }
        ec.clear();
        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::none, ec);
        if (ec) { outErr = "Failed to copy file."; Logf("MoveEntryToDir: FAIL copy file '%s' -> '%s': %s",
                                                        src.string().c_str(), dest.string().c_str(), ec.message().c_str()); return false; }
        ec.clear();
        std::filesystem::remove(src, ec);
        if (ec) { outErr = "Copied, but failed to delete source file."; Logf("MoveEntryToDir: FAIL remove src: %s", ec.message().c_str()); return false; }
        Logf("MoveEntryToDir: file copy+delete OK");
        return true;
    }
}


static bool WriteAceproj(const std::filesystem::path& aceprojPath, const std::string& name, const std::string& version) {
    json j{{"Name", name},{"EngineVersion", version},{"Modules", json::array()},{"Plugins", json::array()}};
    std::ofstream out(aceprojPath);
    if (!out) return false;
    out << j.dump(2);
    return true;
}

static bool NewProjectFromTemplate_Basic2D(const std::filesystem::path& pickedFolder, std::filesystem::path& outAceproj) {
    try {
        const auto src = TemplatesDir() / "Basic2D";
        if (!std::filesystem::exists(src)) { std::printf("[ACEEditor] Template not found: %s\n", src.string().c_str()); return false; }
        std::filesystem::path destRoot = pickedFolder;
        if (!std::filesystem::exists(destRoot)) std::filesystem::create_directories(destRoot);
        bool isEmpty = true; try { isEmpty = std::filesystem::is_empty(destRoot); } catch (...) {}
        if (!isEmpty) destRoot = MakeUniqueChildDirectory(destRoot, "NewACEProject");
        std::filesystem::create_directories(destRoot);
        std::printf("[ACEEditor] Creating project at: %s\n", destRoot.string().c_str());
        if (!CopyDirectoryContents(src, destRoot)) { std::printf("[ACEEditor] Copy failed\n"); return false; }
        const auto templProj = destRoot / "Template.aceproj"; if (std::filesystem::exists(templProj)) std::filesystem::remove(templProj);
        const std::string projName = destRoot.filename().string();
        outAceproj = destRoot / (projName + ".aceproj");
        if (!WriteAceproj(outAceproj, projName, "0.1.0")) return false;
        std::error_code ec;
        std::filesystem::create_directories(destRoot / "Content", ec);
        std::filesystem::create_directories(destRoot / "Source",  ec);
        std::printf("[ACEEditor] New project created at: %s\n", destRoot.string().c_str());
        return true;
    } catch (const std::exception& e) {
        std::printf("[ACEEditor] NewProject error: %s\n", e.what());
        return false;
    }
}



// ----- Settings (Recent + CB prefs) -----

static std::filesystem::path AppDataDir() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) return std::filesystem::path(appdata) / "ACE";
#endif
    return GetExeDir() / "ACE";
}
static std::filesystem::path SettingsPath() { return AppDataDir() / "settings.json"; }

#ifndef ACE_ENABLE_LOG
#define ACE_ENABLE_LOG 1
#endif

static std::filesystem::path LogFilePath() {
    std::error_code ec;
    std::filesystem::create_directories(AppDataDir(), ec);
    return AppDataDir() / "ace_editor.log";
}

static void Logf(const char* fmt, ...) {
#if ACE_ENABLE_LOG
    char msg[2048];
    va_list args; va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char ts[64];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    std::string line = std::string("[") + ts + "] " + msg + "\n";
    std::fputs(line.c_str(), stdout);
#ifdef _WIN32
    OutputDebugStringA(line.c_str());
#endif
    std::ofstream out(LogFilePath(), std::ios::binary | std::ios::app);
    if (out) out.write(line.data(), (std::streamsize)line.size());
#endif
}


static void LoadSettings(EditorState& S) {
    const auto path = SettingsPath();
    if (!std::filesystem::exists(path)) return;
    try {
        std::ifstream in(path); if (!in) return;
        json j; in >> j;

        // Recent
        S.Recent.clear();
        if (j.contains("Recent") && j["Recent"].is_array()) {
            for (auto& item : j["Recent"]) if (item.is_string()) {
                std::filesystem::path p(item.get<std::string>());
                if (std::filesystem::exists(p)) S.Recent.push_back(p);
            }
        }

        // Content Browser prefs
        if (j.contains("CB")) {
            const auto& cb = j["CB"];
            if (cb.contains("Thumb") && cb["Thumb"].is_number()) S.CB.ThumbnailSize = (float)cb["Thumb"].get<double>();
            if (cb.contains("LastFolder") && cb["LastFolder"].is_string()) S.CB.LastFolder = cb["LastFolder"].get<std::string>();
        }

        // Build selection (new)
        if (j.contains("BuildSel") && j["BuildSel"].is_object()) {
            const auto& b = j["BuildSel"];
            if (b.contains("Config")   && b["Config"].is_string())   { EBuildConfig   v; if (FromStr(b["Config"].get<std::string>(),   v)) S.BuildSel.Config = v; }
            if (b.contains("Target")   && b["Target"].is_string())   { EBuildTarget   v; if (FromStr(b["Target"].get<std::string>(),   v)) S.BuildSel.Target = v; }
            if (b.contains("Platform") && b["Platform"].is_string()) { EBuildPlatform v; if (FromStr(b["Platform"].get<std::string>(), v)) S.BuildSel.Platform = v; }
            if (b.contains("OpenOut")  && b["OpenOut"].is_boolean()) S.BuildSel.bOpenOutputOnSuccess = b["OpenOut"].get<bool>();
            if (b.contains("Jobs")     && b["Jobs"].is_number_integer()) S.BuildSel.ParallelJobs = b["Jobs"].get<int>();
            if (b.contains("Extra")    && b["Extra"].is_string())    S.BuildSel.ExtraArgs = b["Extra"].get<std::string>();
        }
    } catch (...) {}
}

static void SaveSettings(const EditorState& S) {
    std::error_code ec;
    std::filesystem::create_directories(AppDataDir(), ec);

    json root;

    // Recent projects
    root["Recent"] = json::array();
    for (auto& p : S.Recent) root["Recent"].push_back(p.string());

    // Content Browser prefs
    json jcb;
    jcb["Thumb"]      = S.CB.ThumbnailSize;
    jcb["LastFolder"] = S.CB.LastFolder.string();
    root["CB"]        = jcb;

    // Build prefs (explicit object construction fixes the initializer error)
    json jBuild;
    jBuild["Config"]   = ToStr(S.BuildSel.Config);
    jBuild["Target"]   = ToStr(S.BuildSel.Target);
    jBuild["Platform"] = ToStr(S.BuildSel.Platform);
    jBuild["OpenOut"]  = S.BuildSel.bOpenOutputOnSuccess;
    jBuild["Jobs"]     = S.BuildSel.ParallelJobs;
    jBuild["Extra"]    = S.BuildSel.ExtraArgs;
    root["BuildSel"]      = jBuild;

    std::ofstream out(SettingsPath());
    if (!out) return;
    out << root.dump(2);
}

static void AddRecentProject(EditorState& S, const std::filesystem::path& p) {
    auto norm = std::filesystem::weakly_canonical(p);
    S.Recent.erase(std::remove_if(S.Recent.begin(), S.Recent.end(),
        [&](const std::filesystem::path& x){ return PathsEqual(x, norm); }), S.Recent.end());
    S.Recent.insert(S.Recent.begin(), norm);
    if (S.Recent.size() > 10) S.Recent.resize(10);
    SaveSettings(S);
}

// ----- Project Settings -----

static void OpenProjectSettings(EditorState& S) {
    if (!S.Project) return;
    const auto& info = S.Project->GetInfo();
    S.NameBuf    = info.Name;
    S.VersionBuf = info.EngineVersion;
    S.SettingsDirty = false;
    S.ShowProjectSettings = true;
}
static bool SaveProjectFileWithOverrides(const EditorState& S, const std::string& name, const std::string& version) {
    if (!S.Project) return false;
    const auto& info = S.Project->GetInfo();
    json j; j["Name"]=name; j["EngineVersion"]=version; j["Modules"]=info.Modules; j["Plugins"]=info.Plugins;
    std::ofstream out(S.ProjectFile); if (!out) return false; out << j.dump(2); return true;
}
static void DrawProjectSettings(EditorState& S) {
    if (!S.ShowProjectSettings) return; if (!S.Project) { S.ShowProjectSettings = false; return; }
    ImGui::SetNextWindowSize(ImVec2(520, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Project Settings", &S.ShowProjectSettings, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Project file: %s", S.ProjectFile.string().c_str()); ImGui::Separator();
        char nameBuf[256]; std::snprintf(nameBuf, sizeof(nameBuf), "%s", S.NameBuf.c_str());
        char verBuf[64];   std::snprintf(verBuf, sizeof(verBuf), "%s", S.VersionBuf.c_str());
        if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) { S.NameBuf = nameBuf; S.SettingsDirty = true; }
        if (ImGui::InputText("Engine Version", verBuf, IM_ARRAYSIZE(verBuf))) { S.VersionBuf = verBuf; S.SettingsDirty = true; }
        ImGui::Spacing();
        if (ImGui::Button("Save") && S.SettingsDirty) {
            if (SaveProjectFileWithOverrides(S, S.NameBuf, S.VersionBuf)) { S.Project = ace::Project::Load(S.ProjectFile); S.SettingsDirty = false; }
        }
        ImGui::SameLine(); if (ImGui::Button("Close")) S.ShowProjectSettings = false;
        if (!S.SettingsDirty) { ImGui::SameLine(); ImGui::TextDisabled("(no changes)"); }
    }
    ImGui::End();
}

// ----- Content Browser helpers -----

static bool IsSubPathOf(const std::filesystem::path& base, const std::filesystem::path& p) {
    std::error_code ec;
    auto bp = std::filesystem::weakly_canonical(base, ec);
    auto pp = std::filesystem::weakly_canonical(p, ec);
    if (ec) return false;
    auto bi = bp.begin(), pi = pp.begin();
    for (; bi != bp.end() && pi != pp.end(); ++bi, ++pi) if (*bi != *pi) return false;
    return std::distance(bp.begin(), bp.end()) <= std::distance(pp.begin(), pp.end());
}

static void EnsureContentRoot(EditorState& S) {
    if (!S.Project) { S.CB.Root.clear(); S.CB.Current.clear(); return; }
    const auto root = S.Project->ContentDir(); // abs
    if (S.CB.Root != root || S.CB.Root.empty()) {
        S.CB.Root = root;
        // use persisted last folder if still inside Content
        if (!S.CB.LastFolder.empty() && IsSubPathOf(root, S.CB.LastFolder) && std::filesystem::exists(S.CB.LastFolder))
            S.CB.Current = S.CB.LastFolder;
        else
            S.CB.Current = root;

        // Clear transient UI state
        S.CB.Selection.clear();
        S.CB.Error.clear();
        S.CB.Filter.clear();

        std::memset(S.CB.NameBuf,   0, sizeof(S.CB.NameBuf));
        std::memset(S.CB.ExtBuf,    0, sizeof(S.CB.ExtBuf));
        std::memset(S.CB.RenameBuf, 0, sizeof(S.CB.RenameBuf));

        // New Item dialog state (new)
        S.CB.ShowNewItemDialog = false;
        S.CB.NewItemError.clear();
        std::memset(S.CB.NewItemNameBuf,    0, sizeof(S.CB.NewItemNameBuf));
        std::memset(S.CB.NewItemTemplateId, 0, sizeof(S.CB.NewItemTemplateId));
        S.CB.NewItemTargetFolder.clear();
    }
}


static bool IsValidName(const std::string& name) {
    if (name.empty()) return false;
#ifdef _WIN32
    static const char* bad = "\\/:*?\"<>|";
#else
    static const char* bad = "/";
#endif
    if (name.find_first_of(bad) != std::string::npos) return false;
    if (name == "." || name == "..") return false;
    return true;
}
static std::string TruncMiddle(const std::string& s, int maxChars) {
    if ((int)s.size() <= maxChars) return s;
    if (maxChars <= 3) return s.substr(0, maxChars);
    int keep = (maxChars - 3) / 2;
    return s.substr(0, keep) + "..." + s.substr(s.size()-keep);
}

static void SelectClear(ContentBrowserState& CB) { CB.Selection.clear(); }
static bool IsSelected(const ContentBrowserState& CB, const std::filesystem::path& p) {
    auto s = CanonicalStr(p);
    for (auto& e : CB.Selection) if (CanonicalStr(e) == s) return true;
    return false;
}
static void SelectAdd(ContentBrowserState& CB, const std::filesystem::path& p) {
    if (!IsSelected(CB, p)) CB.Selection.push_back(p);
}
static void SelectRemove(ContentBrowserState& CB, const std::filesystem::path& p) {
    auto s = CanonicalStr(p);
    CB.Selection.erase(std::remove_if(CB.Selection.begin(), CB.Selection.end(),
        [&](const std::filesystem::path& x){ return CanonicalStr(x) == s; }), CB.Selection.end());
}
static void SelectSet(ContentBrowserState& CB, const std::filesystem::path& p) {
    CB.Selection.clear(); CB.Selection.push_back(p);
}

// ---------- Editors: Text & Blueprint ----------

static bool IsTextLike(const std::filesystem::path& p) {
    std::string ext = ToLower(p.extension().string());
    static const std::unordered_set<std::string> exts = {
        // plain text + configs
        ".txt",".md",".json",".ini",".cfg",".csv",".xml",
        // code-ish
        ".h",".hpp",".hh",".c",".cpp",".cc",".cxx",
        ".glsl",".vert",".frag",".geom",".comp",
        // ACE assets
        ".aceasset",         // already there
        ".blueprint",        // NEW
        ".gamemode",         // NEW
        ".asset"             // NEW (generic data asset)
    };
    return exts.count(ext) > 0;
}
static bool IsBlueprintFile(const std::filesystem::path& p) {
    return ToLower(p.extension().string()) == ".blueprint";
}

static bool LoadFileToString(const std::filesystem::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    std::string tmp;
    tmp.resize((size_t)in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(tmp.data(), (std::streamsize)tmp.size());
    out.swap(tmp);
    return true;
}
static bool SaveStringToFile(const std::filesystem::path& p, const std::string& content) {
    std::error_code ec; std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), (std::streamsize)content.size());
    return true;
}

// --- Blueprint load/save (very simple JSON) ---
static void EnsureDefaultGraph(bp::Graph& g)
{
    if (!g.nodes.empty()) return;
    // Two constants and an adder
    bp::Node n1; n1.id = g.NewId(); n1.title = "Const 2"; n1.pos = ImVec2(80,80);
    n1.outputs.push_back({g.NewId(), "Value", bp::PinKind::Output, bp::ValueType::Float});

    bp::Node n2; n2.id = g.NewId(); n2.title = "Const 3"; n2.pos = ImVec2(80,200);
    n2.outputs.push_back({g.NewId(), "Value", bp::PinKind::Output, bp::ValueType::Float});

    bp::Node add; add.id = g.NewId(); add.title = "Add (Float)"; add.pos = ImVec2(340,140);
    add.inputs.push_back({g.NewId(), "A", bp::PinKind::Input, bp::ValueType::Float});
    add.inputs.push_back({g.NewId(), "B", bp::PinKind::Input, bp::ValueType::Float});
    add.outputs.push_back({g.NewId(), "Result", bp::PinKind::Output, bp::ValueType::Float});

    g.nodes.push_back(n1); g.nodes.push_back(n2); g.nodes.push_back(add);
}

static bool LoadBlueprint(const std::filesystem::path& path, bp::Graph& g)
{
    g = bp::Graph{};
    std::string txt;
    if (!LoadFileToString(path, txt) || txt.empty()) { EnsureDefaultGraph(g); return true; }
    try {
        auto j = json::parse(txt);
        // Optional wrapper
        auto gj = j.contains("Graph") ? j["Graph"] : j;
        if (gj.contains("nextId")) g.nextId = gj["nextId"].get<int>();
        if (gj.contains("nodes")) {
            for (auto& jn : gj["nodes"]) {
                bp::Node n;
                n.id = jn.value("id", 0);
                n.title = jn.value("title", "Node");
                auto jp = jn["pos"];
                n.pos = ImVec2(jp.value("x", 0.0f), jp.value("y", 0.0f));
                if (jn.contains("inputs"))
                    for (auto& jpins : jn["inputs"])
                        n.inputs.push_back({ jpins.value("id",0), jpins.value("name","In"),
                                             bp::PinKind::Input, bp::ValueType::Float });
                if (jn.contains("outputs"))
                    for (auto& jpins : jn["outputs"])
                        n.outputs.push_back({ jpins.value("id",0), jpins.value("name","Out"),
                                              bp::PinKind::Output, bp::ValueType::Float });
                g.nodes.push_back(std::move(n));
            }
        }
        if (gj.contains("links")) {
            for (auto& jl : gj["links"]) {
                bp::Link l;
                l.id = jl.value("id", 0);
                l.fromNode = jl.value("fromNode", 0);
                l.fromPin  = jl.value("fromPin",  0);
                l.toNode   = jl.value("toNode",   0);
                l.toPin    = jl.value("toPin",    0);
                g.links.push_back(l);
            }
        }
        if (g.nextId <= 0) {
            // recover nextId
            int mx = 0;
            for (auto& n : g.nodes) {
                mx = std::max(mx, n.id);
                for (auto& p : n.inputs)  mx = std::max(mx, p.id);
                for (auto& p : n.outputs) mx = std::max(mx, p.id);
            }
            for (auto& l : g.links) mx = std::max(mx, l.id);
            g.nextId = mx + 1;
        }
    } catch (...) {
        EnsureDefaultGraph(g);
    }
    return true;
}
static bool SaveBlueprint(const std::filesystem::path& path, const bp::Graph& g)
{
    json gj;
    gj["nextId"] = g.nextId;
    gj["nodes"] = json::array();
    for (auto& n : g.nodes) {
        json jn;
        jn["id"] = n.id;
        jn["title"] = n.title;
        jn["pos"] = { {"x", n.pos.x}, {"y", n.pos.y} };
        jn["inputs"]  = json::array();
        jn["outputs"] = json::array();
        for (auto& p : n.inputs)  jn["inputs"].push_back( { {"id", p.id}, {"name", p.name} } );
        for (auto& p : n.outputs) jn["outputs"].push_back( { {"id", p.id}, {"name", p.name} } );
        gj["nodes"].push_back(jn);
    }
    gj["links"] = json::array();
    for (auto& l : g.links) gj["links"].push_back( { {"id", l.id}, {"fromNode", l.fromNode}, {"fromPin", l.fromPin},
                                                     {"toNode", l.toNode}, {"toPin", l.toPin} } );
    // Optional top wrapper with Type/Name
    json root;
    root["Type"] = "Blueprint";
    root["Name"] = path.stem().string();
    root["Graph"] = gj;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << root.dump(2);
    return true;
}

// --- Open tabs ---

static void OpenTextTab(EditorState& S, const std::filesystem::path& p) {
    for (int i = 0; i < (int)S.Tabs.size(); ++i) {
        if (PathsEqual(S.Tabs[i].Path, p)) {
            S.ActiveTab = i;
            S.FocusNewTab = true;
            Logf("OpenTextTab: focus existing idx=%d '%s'", i, p.string().c_str());
            return;
        }
    }
    EditorTab t;
    t.Type  = EditorTabType::Text;
    t.Path  = p;
    t.Title = p.filename().string();
    t.Dirty = false;
    t.ReadOnly = false;

    if (std::filesystem::exists(p)) {
        if (!LoadFileToString(p, t.Buffer)) t.Buffer.clear();
    } else {
        t.Buffer.clear();
        t.Dirty = true; // unsaved
    }

    S.Tabs.push_back(std::move(t));
    S.ActiveTab   = (int)S.Tabs.size() - 1;
    S.FocusNewTab = true;
    Logf("OpenTextTab: created idx=%d totalTabs=%d '%s'",
         S.ActiveTab, (int)S.Tabs.size(), p.string().c_str());
}

static void OpenBlueprintTab(EditorState& S, const std::filesystem::path& p)
{
    for (int i = 0; i < (int)S.Tabs.size(); ++i) {
        if (PathsEqual(S.Tabs[i].Path, p)) {
            S.ActiveTab = i; S.FocusNewTab = true;
            Logf("OpenBlueprintTab: focus existing idx=%d '%s'", i, p.string().c_str());
            return;
        }
    }
    EditorTab t;
    t.Type    = EditorTabType::Blueprint;
    t.Path    = p;
    t.Title   = p.filename().string();
    t.BPLoaded = LoadBlueprint(p, t.BPGraph);
    t.BPDirty  = false;

    S.Tabs.push_back(std::move(t));
    S.ActiveTab   = (int)S.Tabs.size()-1;
    S.FocusNewTab = true;
    Logf("OpenBlueprintTab: created idx=%d totalTabs=%d loaded=%d '%s'",
         S.ActiveTab, (int)S.Tabs.size(), (int)S.Tabs.back().BPLoaded, p.string().c_str());
}

static void OpenFileInEditor(EditorState& S, const std::filesystem::path& p) {
    S.P.Editors = true; // ensure Editors panel is visible next frame
    Logf("OpenFileInEditor: '%s' ext='%s'", p.string().c_str(), p.extension().string().c_str());

    if (IsBlueprintFile(p)) {
        Logf("OpenFileInEditor: IsBlueprintFile -> OpenBlueprintTab");
        OpenBlueprintTab(S, p);
        return;
    }
    if (IsTextLike(p)) {
        Logf("OpenFileInEditor: IsTextLike -> OpenTextTab");
        OpenTextTab(S, p);
        return;
    }
    Logf("OpenFileInEditor: Fallback -> OpenTextTab");
    OpenTextTab(S, p);
}


// --- Blueprint canvas drawing ---

static ImVec2 CanvasFromScreen(const ImVec2& screen, const ImVec2& origin, const ImVec2& pan)
{
    return ImVec2( (screen.x - origin.x) - pan.x, (screen.y - origin.y) - pan.y );
}
static ImVec2 ScreenFromCanvas(const ImVec2& canvas, const ImVec2& origin, const ImVec2& pan)
{
    return ImVec2( origin.x + pan.x + canvas.x, origin.y + pan.y + canvas.y );
}
static void DrawSpline(ImDrawList* dl, ImVec2 a, ImVec2 b, float thickness=2.0f)
{
    // Simple horizontal cubic-looking curve
    const float dx = (b.x - a.x) * 0.5f;
    ImVec2 p1 = a + ImVec2(+dx, 0);
    ImVec2 p2 = b + ImVec2(-dx, 0);
    dl->AddBezierCubic(a, p1, p2, b, IM_COL32(200,220,255,255), thickness);
}

static void DrawBlueprintEditor(EditorTab& tab)
{
    auto& g  = tab.BPGraph;
    auto& ui = tab.BpUI;

    // Canvas
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::BeginChild("BP.Canvas", ImVec2(0,0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 origin = ImGui::GetWindowPos();
    ImVec2 size   = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Pan with middle mouse
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        ui.pan += ImGui::GetIO().MouseDelta;
    }

    // Background grid
    const float grid = 32.0f;
    for (float x = fmodf(ui.pan.x, grid); x < size.x; x += grid)
        dl->AddLine(ImVec2(origin.x + x, origin.y),
                    ImVec2(origin.x + x, origin.y + size.y), IM_COL32(40,40,40,255));
    for (float y = fmodf(ui.pan.y, grid); y < size.y; y += grid)
        dl->AddLine(ImVec2(origin.x, origin.y + y),
                    ImVec2(origin.x + size.x, origin.y + y), IM_COL32(40,40,40,255));

    // Precompute node rects and pin positions
    struct NodeDraw {
        bp::Node* n;
        ImVec2    pos;      // top-left in screen space
        ImVec2    size;
        std::vector<ImVec2> inPinPos;
        std::vector<ImVec2> outPinPos;
    };
    std::vector<NodeDraw> draws; draws.reserve(g.nodes.size());

    auto calcNodeSize = [](const bp::Node& n)->ImVec2{
        int rows = (int)std::max(n.inputs.size(), n.outputs.size());
        float w = 160.0f;
        w = std::max(w, ImGui::CalcTextSize(n.title.c_str()).x + 40.0f); // widen for long titles
        float h = 36.0f + rows * 22.0f + 8.0f;
        return ImVec2(w, h);
    };

    for (auto& n : g.nodes) {
        NodeDraw nd; nd.n = &n;
        nd.size = calcNodeSize(n);
        nd.pos  = ScreenFromCanvas(n.pos, origin, ui.pan);
        // pins
        nd.inPinPos.resize(n.inputs.size());
        nd.outPinPos.resize(n.outputs.size());
        float y0 = nd.pos.y + 36.0f;
        for (size_t i=0;i<n.inputs.size(); ++i)  nd.inPinPos[i]  = ImVec2(nd.pos.x,               y0 + i*22.0f);
        for (size_t i=0;i<n.outputs.size(); ++i) nd.outPinPos[i] = ImVec2(nd.pos.x + nd.size.x,   y0 + i*22.0f);
        draws.push_back(std::move(nd));
    }

    // Links (behind nodes)
    for (auto& l : g.links) {
        auto* nA = g.FindNode(l.fromNode);
        auto* nB = g.FindNode(l.toNode);
        if (!nA || !nB) continue;

        ImVec2 from = ImVec2(0,0), to = ImVec2(0,0);
        for (auto& nd : draws) {
            if (nd.n->id == l.fromNode) {
                for (size_t i=0;i<nd.n->outputs.size(); ++i)
                    if (nd.n->outputs[i].id == l.fromPin) from = nd.outPinPos[i];
            }
            if (nd.n->id == l.toNode) {
                for (size_t i=0;i<nd.n->inputs.size(); ++i)
                    if (nd.n->inputs[i].id == l.toPin) to = nd.inPinPos[i];
            }
        }
        DrawSpline(dl, from, to, 2.0f);
    }

    // Node widgets (front): draw boxes + handle drag & pin hit-test
    ImGui::PushStyleColor(ImGuiCol_ChildBg, 0); // ensure child content invisible
    int nodeToFront = -1;

    // Helper: determine if a (node,pin) pair is an output pin
    auto isOutputPin = [&](int nodeId, int pinId)->bool {
        if (const bp::Node* n = g.FindNode(nodeId)) {
            for (auto& p : n->outputs) if (p.id == pinId) return true;
        }
        return false;
    };

    for (size_t i=0;i<draws.size(); ++i) {
        auto& nd = draws[i];
        auto* n  = nd.n;

        // Node body
        ImU32 bg = (ui.selectedNode == n->id) ? IM_COL32(70,90,130,255) : IM_COL32(55,55,60,255);
        dl->AddRectFilled(nd.pos, nd.pos + nd.size, bg, 6.0f);
        dl->AddRect(nd.pos, nd.pos + nd.size, IM_COL32(160,160,160,200), 6.0f);

        // Title bar
        ImVec2 titleMin = nd.pos;
        ImVec2 titleMax = ImVec2(nd.pos.x + nd.size.x, nd.pos.y + 28.0f);
        dl->AddRectFilled(titleMin, titleMax, IM_COL32(80,80,85,255), 6.0f, ImDrawFlags_RoundCornersTop);
        dl->AddText(ImVec2(titleMin.x + 8.0f, titleMin.y + 6.0f), IM_COL32_WHITE, n->title.c_str());

        // Hit box for dragging node
        ImGui::SetCursorScreenPos(nd.pos);
        ImGui::PushID(n->id);
        ImGui::InvisibleButton("node", nd.size);
        bool nodeActive = ImGui::IsItemActive();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            ui.selectedNode = n->id;
            nodeToFront = (int)i;
        }
        if (nodeActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ui.linking) {
            n->pos += ImGui::GetIO().MouseDelta;   // move in canvas space (1:1 with screen delta)
            tab.BPDirty = true;
        }

        // Pins + linking
        auto drawPin = [&](const ImVec2& p, bool isOutput, int pinId, const char* label, bool rightAlignLabel)
        {
            // Pin circle
            dl->AddCircleFilled(p, 5.0f, IM_COL32(220,220,240,255));
            ImGui::SetCursorScreenPos(p - ImVec2(6,6));
            ImGui::PushID(pinId);
            ImGui::InvisibleButton("pin", ImVec2(12,12));   // <-- fixed ID usage (no temporary std::string)
            bool hovered = ImGui::IsItemHovered();
            if (hovered) dl->AddCircle(p, 7.0f, IM_COL32(255,230,120,255), 0, 2.0f);

            // Start/finish link on activation (mouse down)
            if (ImGui::IsItemActivated()) {
                if (!ui.linking) {
                    // Start from either side (input or output). We'll resolve direction on finish.
                    ui.linking     = true;
                    ui.linkFromNode= n->id;
                    ui.linkFromPin = pinId;
                    ui.linkFromPos = p;
                } else {
                    // Finish: if opposite side -> connect, else restart from this pin.
                    bool startedWasOutput = isOutputPin(ui.linkFromNode, ui.linkFromPin);
                    bool finishIsOutput   = isOutput;
                    if (startedWasOutput != finishIsOutput) {
                        // Build a link with correct direction: output -> input
                        bp::Link l;
                        l.id = g.NewId();
                        if (startedWasOutput) {
                            l.fromNode = ui.linkFromNode; l.fromPin = ui.linkFromPin;
                            l.toNode   = n->id;           l.toPin   = pinId;
                        } else {
                            l.fromNode = n->id;           l.fromPin = pinId;
                            l.toNode   = ui.linkFromNode; l.toPin   = ui.linkFromPin;
                        }
                        // Prevent duplicate links
                        bool dup = false;
                        for (auto& e : g.links) {
                            if (e.fromNode==l.fromNode && e.fromPin==l.fromPin &&
                                e.toNode==l.toNode && e.toPin==l.toPin) { dup = true; break; }
                        }
                        if (!dup) { g.links.push_back(l); tab.BPDirty = true; }
                        ui.linking = false;
                    } else {
                        // Same side: restart from this pin
                        ui.linkFromNode = n->id;
                        ui.linkFromPin  = pinId;
                        ui.linkFromPos  = p;
                    }
                }
            }
            ImGui::PopID();

            // Pin labels
            if (label && *label) {
                if (rightAlignLabel) {
                    ImVec2 ts = ImGui::CalcTextSize(label);
                    dl->AddText(p - ImVec2(8 + ts.x, 6), IM_COL32(200,200,200,255), label);
                } else {
                    dl->AddText(p + ImVec2(8, -6), IM_COL32(200,200,200,255), label);
                }
            }
        };

        for (size_t k=0;k<n->inputs.size(); ++k)
            drawPin(nd.inPinPos[k], false, n->inputs[k].id, n->inputs[k].name.c_str(), false);
        for (size_t k=0;k<n->outputs.size(); ++k)
            drawPin(nd.outPinPos[k], true,  n->outputs[k].id, n->outputs[k].name.c_str(), true);

        ImGui::PopID(); // node id
    }
    ImGui::PopStyleColor();

    // Bring dragged/selected node to front (simple re-order)
    if (nodeToFront >= 0) {
        auto n = g.nodes[nodeToFront];
        g.nodes.erase(g.nodes.begin() + nodeToFront);
        g.nodes.push_back(n);
    }

    // While linking, draw preview line
    if (ui.linking) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        DrawSpline(dl, ui.linkFromPos, mouse, 2.0f);
        // Cancel with right click or Escape
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ui.linking = false;
        }
    }

    // Delete selected node with Delete key (remove attached links)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && ui.selectedNode != 0) {
            int nid = ui.selectedNode;
            // erase links
            g.links.erase(std::remove_if(g.links.begin(), g.links.end(),
                [&](const bp::Link& l){ return l.fromNode==nid || l.toNode==nid; }), g.links.end());
            // erase node
            g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(),
                [&](const bp::Node& n){ return n.id==nid; }), g.nodes.end());
            ui.selectedNode = 0;
            tab.BPDirty = true;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}



// --- Editors panel (tabs) ---

static void DrawPanel_Editors(EditorState& S) {
    if (!S.P.Editors) return;

    // ---------------- Focus/visibility control (fixes popup blocking) ----------------
    // We only request focus once on meaningful events (new tab, tab switch, explicit FocusNewTab).
    // Never re-focus every frame while tabs exist (that prevents other popups/menus).
    static bool s_shown_once = false;
    static int  s_last_tab_count = -1;
    static int  s_last_active_tab = -999;
    static bool s_request_focus = false;

    const int  tab_count  = (int)S.Tabs.size();
    const bool has_tabs   = tab_count > 0;
    const bool new_tab    = (tab_count > s_last_tab_count);
    const bool tab_switched = (S.ActiveTab != s_last_active_tab && S.ActiveTab >= 0);

    if (S.FocusNewTab || new_tab || tab_switched || !s_shown_once) {
        // Only focus on true change; don't spam every frame.
        s_request_focus = S.FocusNewTab || new_tab || tab_switched;
        // Make sure the window is uncollapsed at least once.
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
        if (s_request_focus) {
            ImGui::SetNextWindowFocus();           // one-shot focus request
        }
        ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    }

    if (!ImGui::Begin("Editors", &S.P.Editors)) {
        // When hidden/collapsed, clear one-shot so we don't keep stealing focus.
        s_request_focus = false;
        ImGui::End();
        return;
    }
    s_shown_once = true;

    // Keep ActiveTab valid when tabs exist
    if (has_tabs && (S.ActiveTab < 0 || S.ActiveTab >= tab_count)) {
        S.ActiveTab = 0;
        S.FocusNewTab = true;      // trigger a one-shot focus on next frame
    }

    // ---------------- Toolbar for active tab ----------------
    if (S.ActiveTab >= 0 && S.ActiveTab < tab_count) {
        EditorTab& tab = S.Tabs[S.ActiveTab];

        if (tab.Type == EditorTabType::Text) {
            if (ImGui::Button("Save (Ctrl+S)")) {
                if (SaveStringToFile(tab.Path, tab.Buffer)) tab.Dirty = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload")) {
                std::string tmp;
                if (LoadFileToString(tab.Path, tmp)) { tab.Buffer = std::move(tmp); tab.Dirty = false; }
            }
        } else { // Blueprint
            if (ImGui::Button("Save (Ctrl+S)")) {
                if (SaveBlueprint(tab.Path, tab.BPGraph)) tab.BPDirty = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Revert")) {
                LoadBlueprint(tab.Path, tab.BPGraph); tab.BPDirty = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Compile")) {
                bool ok = true;
                for (auto& l : tab.BPGraph.links) {
                    ok &= (tab.BPGraph.FindPin(l.fromNode, l.fromPin) != nullptr) &&
                          (tab.BPGraph.FindPin(l.toNode,   l.toPin)   != nullptr);
                }
                ImGui::SameLine();
                ImGui::TextColored(ok ? ImVec4(0.5f,1,0.5f,1) : ImVec4(1,0.5f,0.5f,1),
                                   ok ? "OK" : "Broken links");
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextUnformatted(tab.Path.string().c_str());
    } else {
        if (!has_tabs) {
            ImGui::TextDisabled("No files open.");
            ImGui::SameLine();
            ImGui::TextUnformatted("Open a file from the Content Browser to start editing.");
        } else {
            ImGui::TextDisabled("No active tab selected.");
        }
    }

    ImGui::Separator();

    // ---------------- Shortcuts (save/close) ----------------
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        ImGuiIO& io = ImGui::GetIO();

        // Ctrl+S: Save
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            if (S.ActiveTab >= 0 && S.ActiveTab < tab_count) {
                EditorTab& tab = S.Tabs[S.ActiveTab];
                if (tab.Type == EditorTabType::Text) {
                    if (SaveStringToFile(tab.Path, tab.Buffer)) tab.Dirty = false;
                } else {
                    if (SaveBlueprint(tab.Path, tab.BPGraph)) tab.BPDirty = false;
                }
            }
        }

        // Ctrl+W: Close current tab
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            if (has_tabs && S.ActiveTab >= 0 && S.ActiveTab < tab_count) {
                S.Tabs.erase(S.Tabs.begin() + S.ActiveTab);
                if (S.ActiveTab >= (int)S.Tabs.size()) S.ActiveTab = (int)S.Tabs.size() - 1;
                S.FocusNewTab = true;
            }
        }
    }

    // ---------------- Tabs ----------------
    if (ImGui::BeginTabBar("EditorsTabs",
        ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_TabListPopupButton))
    {
        int toClose = -1;
        for (int i = 0; i < (int)S.Tabs.size(); ++i) {
            EditorTab& tab = S.Tabs[i];
            std::string title = tab.Title;
            if ((tab.Type == EditorTabType::Text && tab.Dirty) ||
                (tab.Type == EditorTabType::Blueprint && tab.BPDirty)) {
                title += " *";
            }

            bool open = true;
            const bool setSelected = (S.ActiveTab == i && S.FocusNewTab);
            if (ImGui::BeginTabItem(title.c_str(), &open,
                                    setSelected ? ImGuiTabItemFlags_SetSelected : 0)) {
                S.ActiveTab = i;
                S.FocusNewTab = false;

                ImVec2 avail = ImGui::GetContentRegionAvail();
                avail.y = std::max(120.0f, avail.y - 2.0f);

                if (tab.Type == EditorTabType::Text) {
                    ImGuiInputTextFlags flags =
                        ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackEdit;
                    if (ImGui::InputTextMultiline("##text", &tab.Buffer, avail, flags)) {
                        tab.Dirty = true;
                    }
                } else {
                    DrawBlueprintEditor(tab);
                }

                ImGui::EndTabItem();
            }
            if (!open) toClose = i;
        }

        if (toClose >= 0) {
            S.Tabs.erase(S.Tabs.begin() + toClose);
            if (S.ActiveTab >= (int)S.Tabs.size()) S.ActiveTab = (int)S.Tabs.size() - 1;
            S.FocusNewTab = true;
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    // ---------------- Update one-shot trackers ----------------
    s_last_tab_count  = (int)S.Tabs.size();
    s_last_active_tab = S.ActiveTab;
    s_request_focus   = false; // consume the one-shot
}




// --- Folder tree (left sidebar inside Content Browser)

static bool DrawFolderTreeNode(const std::filesystem::path& p,
                               const std::filesystem::path& current,
                               std::filesystem::path& outClicked)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    if (PathsEqual(p, current)) flags |= ImGuiTreeNodeFlags_Selected;

    // Detect children
    bool hasChildren = false;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(p, ec);
         !ec && it != std::filesystem::directory_iterator(); ++it)
    {
        if (it->is_directory()) { hasChildren = true; break; }
    }

    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

    const std::string label = p.filename().string();
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked()) outClicked = p;

    // Accept drops onto tree nodes (move/copy into that folder)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pld = ImGui::AcceptDragDropPayload("ACE_PATHS")) {
            const char* data = static_cast<const char*>(pld->Data);
            std::string all(data, data + pld->DataSize);
            std::stringstream ss(all);
            std::string line;
            bool anyErr = false;
            size_t moved = 0;

            Logf("Drop onto TREE folder '%s' payloadSize=%d",
                 p.string().c_str(), (int)pld->DataSize);

            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                std::filesystem::path src(line);
                std::string err;
                Logf("  Move request: '%s' -> '%s'", src.string().c_str(), p.string().c_str());
                if (!MoveEntryToDir(src, p, err)) {
                    anyErr = true;
                    Logf("  Move FAILED: %s", err.c_str());
                } else {
                    ++moved;
                    Logf("  Move OK");
                }
            }
            Logf("Drop onto TREE folder '%s' done. moved=%zu, anyErr=%d", p.string().c_str(), moved, (int)anyErr);
        }
        ImGui::EndDragDropTarget();
    }

    if (open) {
        if (hasChildren) {
            std::vector<std::filesystem::path> kids;
            for (auto it = std::filesystem::directory_iterator(p, ec);
                 !ec && it != std::filesystem::directory_iterator(); ++it)
            {
                if (it->is_directory()) kids.push_back(it->path());
            }
            std::sort(kids.begin(), kids.end(),
                      [](const auto& a, const auto& b){ return a.filename().string() < b.filename().string(); });

            for (auto& c : kids) {
                DrawFolderTreeNode(c, current, outClicked);
            }
        }
        ImGui::TreePop();
    }
    return open;
}

static void Breadcrumbs(EditorState& S) {
    auto& CB = S.CB;
    std::error_code ec_rel;
    auto rel = std::filesystem::relative(CB.Current, CB.Root, ec_rel);
    ImGui::TextDisabled("Content");
    std::filesystem::path walk = CB.Root;
    if (!rel.empty() && !ec_rel) {
        for (auto& part : rel) {
            ImGui::SameLine(); ImGui::TextDisabled(">");
            ImGui::SameLine();
            if (ImGui::SmallButton(part.string().c_str())) {
                walk /= part; CB.Current = walk; SelectClear(CB);
            }
        }
    }
}

static void DrawItemIcon(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1, bool isDir, bool selected) {
    ImU32 bg = isDir ? IM_COL32(64,100,160,255) : IM_COL32(80,80,80,255);
    ImU32 sel= IM_COL32(240,170,0,255);
    dl->AddRectFilled(p0, p1, selected ? sel : bg, 6.0f);
    if (isDir) {
        ImVec2 tab0(p0.x+6, p0.y+6), tab1(p0.x+28, p0.y+18);
        dl->AddRectFilled(tab0, tab1, selected ? IM_COL32(255,210,80,255) : IM_COL32(200,170,80,255), 3.0f);
    }
    dl->AddRect(p0, p1, IM_COL32(200,200,200,80), 6.0f);
}

// ---------- Asset templates ----------

static std::string MakeAssetJson(const std::string& type, const std::string& name) {
    json j;
    j["Type"]    = type;
    j["Name"]    = name;
    j["Version"] = "1.0";
    if (type == "GameMode") {
        j["Properties"] = { {"StartMap", "/Game/Maps/Example"}, {"bAllowRespawn", true} };
    } else if (type == "Material") {
        j["Properties"] = { {"Shader", "DefaultLit"}, {"Params", json::object()} };
    } else if (type == "Blueprint") {
        j["Properties"] = { {"Parent", "Actor"}, {"Components", json::array()} };
    } else {
        j["Properties"] = json::object();
    }
    return j.dump(2);
}

static bool CreateAssetFile(const std::filesystem::path& folder, const std::string& baseName, const std::string& type, std::filesystem::path& outPath) {
    std::string file = baseName + ".aceasset";
    auto p = UniqueSibling(folder, file);
    std::string content = MakeAssetJson(type, std::filesystem::path(p).stem().string());
    if (!SaveStringToFile(p, content)) return false;
    outPath = p;
    return true;
}

// --- Main Content Browser (grid + keyboard + marquee)

// ---- New Item (template-based) helpers ----
static void OpenFileInEditor(EditorState& S, const std::filesystem::path& p); // fwd

struct CBTemplateDesc {
    const char* Id;        // stable id
    const char* Display;   // menu label
    const char* Ext;       // output file extension (with dot)
};

// Ship a tiny starter set. Extend at will.
static const CBTemplateDesc kCBTemplates[] = {
    { "blueprint.graph", "Blueprint (Graph)", ".blueprint" },
    { "asset.gamemode",  "GameMode",          ".gamemode"  },
    { "asset.data",      "Data Asset",        ".asset"     },
};

static int FindCBTemplateIndexById(const char* id) {
    for (int i=0;i<(int)(sizeof(kCBTemplates)/sizeof(kCBTemplates[0]));++i)
        if (std::strcmp(kCBTemplates[i].Id, id) == 0) return i;
    return 0;
}

// State for the modal (kept static so we don't touch ContentBrowserState)
static bool                        g_NewItemOpen = false;
static std::filesystem::path       g_NewItemFolder;
static int                         g_NewItemTplIdx = 0;
static char                        g_NewItemName[256] = {0};

static bool CreateItemFromTemplate(const CBTemplateDesc& T,
                                   const std::filesystem::path& folder,
                                   const std::string& baseName,
                                   std::filesystem::path& outPath,
                                   std::string& outErr,
                                   EditorState& S)
{
    outErr.clear();

    if (!IsValidName(baseName)) { outErr = "Invalid name."; return false; }
    if (!IsSubPathOf(S.CB.Root, folder)) { outErr = "Destination escapes Content root."; return false; }

    // Final file path (unique within folder)
    const std::string fileName = baseName + T.Ext;
    std::filesystem::path dest = UniqueSibling(folder, fileName);

    // Create content based on template
    if (std::strcmp(T.Id, "blueprint.graph") == 0) {
        // Empty .blueprint is OK; loader will populate default graph
        if (!SaveStringToFile(dest, "")) { outErr = "Failed to create blueprint."; return false; }
    } else if (std::strcmp(T.Id, "asset.gamemode") == 0) {
        const std::string content = MakeAssetJson("GameMode", std::filesystem::path(dest).stem().string());
        if (!SaveStringToFile(dest, content)) { outErr = "Failed to create GameMode asset."; return false; }
    } else if (std::strcmp(T.Id, "asset.data") == 0) {
        const std::string content = MakeAssetJson("DataAsset", std::filesystem::path(dest).stem().string());
        if (!SaveStringToFile(dest, content)) { outErr = "Failed to create Data Asset."; return false; }
    } else {
        outErr = "Unknown template.";
        return false;
    }

    outPath = dest;
    return true;
}

static void OpenNewItemDialog(EditorState& S, const char* templateId, const std::filesystem::path& targetFolder)
{
    auto& CB = S.CB;

    // Reset dialog state
    CB.NewItemError.clear();
    std::memset(CB.NewItemNameBuf, 0, sizeof(CB.NewItemNameBuf));
    std::snprintf(CB.NewItemTemplateId, sizeof(CB.NewItemTemplateId), "%s", templateId ? templateId : "");

    // Resolve a valid target folder (fallback to current if invalid)
    std::filesystem::path folder = targetFolder;
    if (folder.empty() || !std::filesystem::exists(folder) || !std::filesystem::is_directory(folder) || !IsSubPathOf(CB.Root, folder)) {
        folder = CB.Current;
    }
    CB.NewItemTargetFolder = folder;

    // Smart default names
    if (std::strcmp(CB.NewItemTemplateId, "blueprint.graph") == 0)
        std::snprintf(CB.NewItemNameBuf, sizeof(CB.NewItemNameBuf), "NewBlueprint");
    else if (std::strcmp(CB.NewItemTemplateId, "asset.gamemode") == 0)
        std::snprintf(CB.NewItemNameBuf, sizeof(CB.NewItemNameBuf), "NewGameMode");
    else if (std::strcmp(CB.NewItemTemplateId, "asset.data") == 0)
        std::snprintf(CB.NewItemNameBuf, sizeof(CB.NewItemNameBuf), "NewDataAsset");
    else
        std::snprintf(CB.NewItemNameBuf, sizeof(CB.NewItemNameBuf), "NewItem");

    CB.ShowNewItemDialog = true;
    ImGui::OpenPopup("New Item");
}


static void DrawNewItemDialog(EditorState& S)
{
    auto& CB = S.CB;
    if (!CB.ShowNewItemDialog)
        return;

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("New Item", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextDisabled("Folder:");
        ImGui::SameLine();
        ImGui::TextUnformatted(CB.NewItemTargetFolder.string().c_str());

        ImGui::Separator();
        ImGui::TextDisabled("Template:");
        ImGui::SameLine();
        ImGui::TextUnformatted(CB.NewItemTemplateId);

        ImGui::Separator();
        ImGui::InputText("Name", CB.NewItemNameBuf, IM_ARRAYSIZE(CB.NewItemNameBuf));
        ImGui::TextDisabled("Note: no extension needed; it is chosen by the template.");

        if (!CB.NewItemError.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "%s", CB.NewItemError.c_str());
        }

        ImGui::Spacing();
        bool create = ImGui::Button("Create");
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancel");

        // Enter submits, Escape cancels
        if (ImGui::IsKeyPressed(ImGuiKey_Enter))  create = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) cancel = true;

        if (cancel) {
            CB.ShowNewItemDialog = false;
            CB.NewItemError.clear();
            ImGui::CloseCurrentPopup();
        }

        if (create) {
            std::string base = CB.NewItemNameBuf;

            // Trim whitespace
            auto trim = [](std::string& s){
                size_t a = 0, b = s.size();
                while (a < b && std::isspace((unsigned char)s[a])) ++a;
                while (b > a && std::isspace((unsigned char)s[b-1])) --b;
                s = (a < b) ? s.substr(a, b - a) : std::string();
            };
            trim(base);

            // Guard rails: non-empty, no path separators, no dot in base name
            if (!IsValidName(base) || base.find('.') != std::string::npos) {
                CB.NewItemError = "Invalid name. Leave out extensions and avoid path characters.";
            } else if (!IsSubPathOf(CB.Root, CB.NewItemTargetFolder)) {
                CB.NewItemError = "Target folder escapes Content/ root.";
            } else {
                std::error_code ec;
                std::filesystem::create_directories(CB.NewItemTargetFolder, ec);

                std::filesystem::path created;
                bool ok = false;
                const char* tid = CB.NewItemTemplateId;

                if (std::strcmp(tid, "blueprint.graph") == 0) {
                    // .blueprint with a default graph
                    auto path = UniqueSibling(CB.NewItemTargetFolder, base + std::string(".blueprint"));
                    bp::Graph g; EnsureDefaultGraph(g);
                    ok = SaveBlueprint(path, g);
                    if (ok) created = path;

                } else if (std::strcmp(tid, "asset.gamemode") == 0) {
                    // Emit .gamemode, not .aceasset
                    auto path = UniqueSibling(CB.NewItemTargetFolder, base + std::string(".gamemode"));
                    const std::string content =
                        "{\n"
                        "  \"type\": \"GameMode\",\n"
                        "  \"base\": \"GameModeBase\",\n"
                        "  \"settings\": {}\n"
                        "}\n";
                    ok = SaveStringToFile(path, content);
                    if (ok) created = path;

                } else if (std::strcmp(tid, "asset.data") == 0) {
                    // Emit .asset, not .aceasset
                    auto path = UniqueSibling(CB.NewItemTargetFolder, base + std::string(".asset"));
                    const std::string content =
                        "{\n"
                        "  \"type\": \"DataAsset\",\n"
                        "  \"class\": \"UDataAsset\",\n"
                        "  \"properties\": {}\n"
                        "}\n";
                    ok = SaveStringToFile(path, content);
                    if (ok) created = path;

                } else {
                    // Fallback: plain text file
                    auto path = UniqueSibling(CB.NewItemTargetFolder, base + std::string(".txt"));
                    ok = SaveStringToFile(path, "");
                    if (ok) created = path;
                }

                if (!ok) {
                    CB.NewItemError = "Failed to create the item on disk.";
                } else {
                    // Success: select it so it shows highlighted in the grid
                    SelectSet(CB, created);
                    CB.Error.clear();
                    CB.NewItemError.clear();
                    CB.ShowNewItemDialog = false;
                    ImGui::CloseCurrentPopup();

                    // Auto-open Blueprints in the editor
                    if (IsBlueprintFile(created)) {
                        S.P.Editors = true;
                        OpenFileInEditor(S, created);
                    }
                }
            }
        }

        ImGui::EndPopup();
    } else {
        // Popup got closed externally
        CB.ShowNewItemDialog = false;
        CB.NewItemError.clear();
    }
}


// --- Full implementation (drop-in) ---
static bool DoCreateNewItem(EditorState& S,
                            const std::string& templateId,
                            const char* nameInput,
                            const std::filesystem::path& targetFolder)
{
    auto& CB = S.CB;

    // Basic guards
    if (!S.Project) {
        CB.NewItemError = "No project loaded.";
        return false;
    }
    if (targetFolder.empty()) {
        CB.NewItemError = "Invalid target folder.";
        return false;
    }
    if (!IsSubPathOf(CB.Root, targetFolder)) {
        CB.NewItemError = "Target folder escapes Content root.";
        return false;
    }

    // Name handling (trim + validate)
    std::string base = nameInput ? std::string(nameInput) : std::string();
    // trim whitespace
    auto trim = [](std::string& s){
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
        s = (a < b) ? s.substr(a, b - a) : std::string();
    };
    trim(base);

    if (base.empty()) {
        CB.NewItemError = "Please enter a name.";
        return false;
    }

    // If the user typed an extension, strip it – we control the extension by template.
    // (e.g., "MyMode.gamemode" -> "MyMode")
    auto dotPos = base.find_last_of('.');
    if (dotPos != std::string::npos) {
        base = base.substr(0, dotPos);
        trim(base);
        if (base.empty()) {
            CB.NewItemError = "Invalid name.";
            return false;
        }
    }

    // Validate base name (re-use existing helper)
    if (!IsValidName(base)) {
        CB.NewItemError = "Invalid characters in name.";
        return false;
    }

    // Map template -> extension + initial content
    std::string ext;
    std::string initial;
    if (templateId == "blueprint.graph") {
        ext = ".blueprint";
        initial =
            "{\n"
            "  \"type\": \"BlueprintGraph\",\n"
            "  \"version\": 1,\n"
            "  \"nodes\": [],\n"
            "  \"links\": []\n"
            "}\n";
    } else if (templateId == "asset.gamemode") {
        ext = ".gamemode";
        initial =
            "{\n"
            "  \"type\": \"GameMode\",\n"
            "  \"base\": \"GameModeBase\",\n"
            "  \"settings\": {}\n"
            "}\n";
    } else if (templateId == "asset.data") {
        ext = ".asset";
        initial =
            "{\n"
            "  \"type\": \"DataAsset\",\n"
            "  \"class\": \"UDataAsset\",\n"
            "  \"properties\": {}\n"
            "}\n";
    } else {
        // Fallback – shouldn't happen from our menus, but be defensive.
        ext = ".txt";
        initial = "";
    }

    // Compose destination path
    std::error_code ec;
    std::filesystem::create_directories(targetFolder, ec); // ensure folder exists
    if (ec) {
        CB.NewItemError = "Failed to create target directory.";
        return false;
    }

    const std::string finalName = base + ext;
    std::filesystem::path dest = targetFolder / finalName;

    // Prefer a unique sibling to avoid hard errors if the name already exists
    if (std::filesystem::exists(dest)) {
        dest = UniqueSibling(targetFolder, finalName);
    }

    // Write file
    {
        std::ofstream out(dest, std::ios::binary);
        if (!out) {
            CB.NewItemError = "Failed to create file.";
            return false;
        }
        out.write(initial.data(), static_cast<std::streamsize>(initial.size()));
        if (!out.good()) {
            CB.NewItemError = "Failed to write file.";
            return false;
        }
    }

    // Success: clear error, select, and open it
    CB.NewItemError.clear();
    CB.Current = targetFolder;
    SelectClear(CB);
    SelectSet(CB, dest);

    // Open in editor (if available)
    try {
        S.P.Editors = true;
        OpenFileInEditor(S, dest);
    } catch (...) {
        // Non-fatal: still created; keep selection so user sees it.
    }

    // Persist last folder (optional – consistent with other code paths)
    CB.LastFolder = CB.Current;
    SaveSettings(S);

    return true;
}

// ---------- C++ Class Wizard helpers ----------

struct CppBaseOption {
    const char* label;   // UI label
    const char* base;    // base class identifier
    const char* include; // header include path (relative to Engine/Source include root)
};

// Pick sensible defaults based on your Engine/Source layout
static const CppBaseOption G_CppBases[] = {
    { "Object",           "Object",           "Runtime/Core/Object.h" },
    { "Actor",            "Actor",            "Runtime/Framework/Actor.h" },
    { "Component",        "Component",        "Runtime/Framework/Component.h" },
    { "GameMode",         "GameMode",         "Runtime/Framework/GameMode.h" },
    { "Pawn",             "Pawn",             "Runtime/Framework/Pawn.h" },
    { "PlayerController", "PlayerController", "Runtime/Framework/PlayerController.h" },
};

static std::filesystem::path ProjectSourceDir(const EditorState& S)
{
    if (S.ProjectFile.empty()) return {};
    return std::filesystem::weakly_canonical(S.ProjectFile.parent_path() / "Source");
}

static bool IsValidCppIdentifier(const std::string& name)
{
    if (name.empty()) return false;
    auto isAlpha  = [](unsigned char c){ return (c=='_') || std::isalpha(c); };
    auto isAlnum_ = [](unsigned char c){ return (c=='_') || std::isalnum(c); };
    if (!isAlpha((unsigned char)name[0])) return false;
    for (size_t i=1;i<name.size();++i) if (!isAlnum_((unsigned char)name[i])) return false;
    return true;
}

static std::string UpperSnake(const std::string& in)
{
    std::string out;
    for (size_t i=0;i<in.size(); ++i) {
        char c = in[i];
        if (std::isupper((unsigned char)c) && i>0) out.push_back('_');
        out.push_back((char)std::toupper((unsigned char)c));
    }
    return out;
}

static std::string GenHeaderForClass(const std::string& className,
                                     const CppBaseOption& base,
                                     const std::string& /*relativeIncludeSelf*/)
{
    std::ostringstream ss;
    ss << "#pragma once\n\n";
    ss << "// Auto-generated by ACE Editor – C++ Class Wizard\n";
    ss << "// Edit includes if your include paths differ.\n\n";

    // Central minimal header (pulls in AceObjectMacros, core types, etc.)
    ss << "#include \"Runtime/Core/AceMinimal.h\"\n";
    ss << "#include \"" << base.include << "\"\n";
    ss << "// If the includes above aren't correct for your project, update them.\n\n";

    // Generated header (created/ensured by EditorCodegen.cpp)
    ss << "#include \"" << className << ".generated.h\"\n\n";

    ss << "class " << className << " : public " << base.base << "\n";
    ss << "{\n";
    ss << "    ACE_CLASS(" << className << ", " << base.base << ")\n";
    ss << "public:\n";
    ss << "    ACE_GENERATED_BODY();\n\n";
    ss << "    " << className << "();\n";
    ss << "    virtual ~" << className << "();\n";
    ss << "};\n";
    return ss.str();
}


static std::string GenSourceForClass(const std::string& className, const std::string& headerRelativeToSourceRoot)
{
    std::ostringstream ss;
    ss << "// Auto-generated by ACE Editor – C++ Class Wizard\n";
    ss << "#include \"" << headerRelativeToSourceRoot << "\"\n\n";
    ss << className << "::" << className << "() = default;\n";
    ss << className << "::~" << className << "() = default;\n";
    return ss.str();
}

// subfolder: optional, can be "" or "Gameplay" etc (relative to /Source)
// outHeaderPath returns the absolute path of the created .h
static bool DoCreateNewCppClass(EditorState& S,
                                const std::string& className,
                                int baseIndex,
                                const std::string& subfolder,
                                std::filesystem::path& outHeaderPath)
{
    outHeaderPath.clear();

    if (!S.Project) { Logf("CppWizard: No project loaded"); return false; }
    if (!IsValidCppIdentifier(className)) { Logf("CppWizard: invalid class name '%s'", className.c_str()); return false; }
    if (baseIndex < 0 || baseIndex >= (int)(sizeof(G_CppBases)/sizeof(G_CppBases[0]))) { Logf("CppWizard: invalid base index"); return false; }

    const auto& base = G_CppBases[baseIndex];

    // Determine /Source/<Subfolder?>
    auto srcRoot = ProjectSourceDir(S);
    if (srcRoot.empty()) { Logf("CppWizard: could not resolve Source directory"); return false; }

    auto targetDir = srcRoot;
    if (!subfolder.empty()) targetDir /= std::filesystem::path(subfolder);

    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);
    if (ec) { Logf("CppWizard: create_directories failed: %s", ec.message().c_str()); return false; }

    auto headerName = className + ".h";
    auto sourceName = className + ".cpp";
    auto headerAbs  = targetDir / headerName;
    auto sourceAbs  = targetDir / sourceName;

    if (std::filesystem::exists(headerAbs) || std::filesystem::exists(sourceAbs)) {
        Logf("CppWizard: files already exist for '%s'", className.c_str());
        return false;
    }

    // Build relative include for source -> header: "<Subfolder/?>Class.h"
    std::filesystem::path headerRelToSource = std::filesystem::relative(headerAbs, srcRoot, ec);
    std::string headerRelStr = ec ? headerName : headerRelToSource.generic_string();

    // Build header content with base include
    // relativeIncludeSelf used only for header guards aesthetics
    std::string includeSelf = headerRelStr;
    std::string h = GenHeaderForClass(className, base, includeSelf);
    std::string c = GenSourceForClass(className, headerRelStr);

    if (!SaveStringToFile(headerAbs, h)) { Logf("CppWizard: failed to write header"); return false; }
    if (!SaveStringToFile(sourceAbs, c)) { Logf("CppWizard: failed to write source"); return false; }
    WriteGeneratedStubIfMissing(headerAbs);
    OpenFileInEditor(S, headerAbs);

    Logf("CppWizard: created '%s' and '%s'", headerAbs.string().c_str(), sourceAbs.string().c_str());
    outHeaderPath = headerAbs;

    // Open header in editor
    OpenFileInEditor(S, headerAbs);

    // Note: if your CMake doesn't use globbing, you'll need to add the new .cpp to your target manually.
    return true;
}




static void DrawPanel_ContentBrowser(EditorState& S) {
    if (!ImGui::Begin("Content Browser")) { ImGui::End(); return; }

    if (!S.Project) {
        ImGui::TextUnformatted("No project loaded");
        ImGui::BulletText("File > Open Project... to select a .aceproj");
        ImGui::BulletText("File > New Project... to create from template");
        ImGui::End();
        return;
    }

    EnsureContentRoot(S);
    auto& CB = S.CB;

    // --- local static state for the C++ class wizard modal (keeps UI simple; no struct changes required)
    static bool   s_ShowNewCppClass = false;
    static int    s_NewCppBaseIdx   = 0;
    static char   s_NewCppName[128] = {};
    static char   s_NewCppSub[256]  = {};   // optional subfolder under /Source
    static std::string s_CppWizardErr;

    // Split: Folder tree (left) | Grid (right)
    ImVec2 full = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("CB.Split", full, false);

    // Left: folder tree with resizable border
    ImGui::BeginChild("CB.Tree", ImVec2(CB.TreeWidth, 0), true);
    {
        // Root node
        std::filesystem::path clicked;
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNodeEx("Content", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth)) {
            std::error_code ec;
            std::vector<std::filesystem::path> roots;
            for (auto it = std::filesystem::directory_iterator(CB.Root, ec);
                 !ec && it != std::filesystem::directory_iterator(); ++it) {
                if (it->is_directory()) roots.push_back(it->path());
            }
            std::sort(roots.begin(), roots.end(),
                      [](const auto& a, const auto& b){ return a.filename().string() < b.filename().string(); });
            for (auto& r : roots) DrawFolderTreeNode(r, CB.Current, clicked);
            ImGui::TreePop();
        }
        ImGui::PopStyleVar();

        if (!clicked.empty()) { CB.Current = clicked; SelectClear(CB); }
    }
    ImGui::EndChild();

    // Vertical resizer
    ImGui::SameLine();
    ImGui::InvisibleButton("CB.TreeResizer", ImVec2(6, -1));
    if (ImGui::IsItemActive())
        CB.TreeWidth += ImGui::GetIO().MouseDelta.x;
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    CB.TreeWidth = std::max(160.0f, std::min(CB.TreeWidth, full.x - 260.0f));

    ImGui::SameLine();

    // Right: grid + toolbar
    ImGui::BeginChild("CB.Right", ImVec2(0, 0), true);

    // Top bar
    Breadcrumbs(S);
    ImGui::Separator();

    // Controls row
    if (ImGui::Button("Up")) {
        if (!CB.Current.empty() && !PathsEqual(CB.Current, CB.Root)) { CB.Current = CB.Current.parent_path(); SelectClear(CB); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) { /* no cache yet */ }
#ifdef _WIN32
    ImGui::SameLine();
    if (ImGui::Button("Reveal")) { RevealInExplorer(CB.Current); }
#endif
    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextUnformatted("Thumbnail Size");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderFloat("##thumb", &CB.ThumbnailSize, 48.0f, 192.0f, "%.0f")) {
        SaveSettings(S);
    }

    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine();
    {
        char tmp[256]{}; std::snprintf(tmp, sizeof(tmp), "%s", CB.Filter.c_str());
        ImGui::SetNextItemWidth(220.f);
        if (ImGui::InputTextWithHint("##filter", "Filter (substring)", tmp, IM_ARRAYSIZE(tmp))) {
            CB.Filter = tmp;
        }
    }

    ImGui::Separator();

    // Scroll area
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (ImGui::BeginChild("CB.Scroll", avail, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs)) {

        // Accept drops on the blank area to move INTO current folder
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pld = ImGui::AcceptDragDropPayload("ACE_PATHS")) {
                const char* data = static_cast<const char*>(pld->Data);
                std::string all(data, data + pld->DataSize);
                std::stringstream ss(all);
                std::string line;
                bool anyErr = false;
                size_t moved = 0;

                Logf("Drop onto BLANK (current='%s') payloadSize=%d",
                     CB.Current.string().c_str(), (int)pld->DataSize);

                while (std::getline(ss, line)) {
                    if (line.empty()) continue;
                    std::filesystem::path src(line);
                    std::string err;
                    Logf("  Move request: '%s' -> '%s'", src.string().c_str(), CB.Current.string().c_str());
                    if (!MoveEntryToDir(src, CB.Current, err)) {
                        anyErr = true;
                        if (!err.empty()) CB.Error = err;
                        Logf("  Move FAILED: %s", err.c_str());
                    } else {
                        ++moved;
                        Logf("  Move OK");
                    }
                }
                if (!anyErr) CB.Error.clear();
                SelectClear(CB);
                Logf("Drop onto BLANK (current='%s') done. moved=%zu, anyErr=%d",
                     CB.Current.string().c_str(), moved, (int)anyErr);
            }
            ImGui::EndDragDropTarget();
        }

        // Keyboard shortcuts (when grid child is focused)
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            ImGuiIO& io = ImGui::GetIO();
            bool ctrl = io.KeyCtrl;

            // Enter -> open first selection (dir navigates, file opens in Editors panel)
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
                if (!CB.Selection.empty()) {
                    auto p = CB.Selection.front();
                    std::error_code ec;
                    if (std::filesystem::is_directory(p, ec)) {
                        CB.Current = p; SelectClear(CB);
                    } else {
                        Logf("Editor: request open (Enter) '%s'", p.string().c_str());
                        S.P.Editors = true; // ensure panel visible
                        OpenFileInEditor(S, p);
                    }
                }
            }
            // Backspace -> go up
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
                if (!PathsEqual(CB.Current, CB.Root)) { CB.Current = CB.Current.parent_path(); SelectClear(CB); }
            }
            // F2 -> rename (single)
            if (ImGui::IsKeyPressed(ImGuiKey_F2, false) && CB.Selection.size() == 1) {
                CB.TargetPath = CB.Selection.front();
                std::snprintf(CB.RenameBuf, sizeof(CB.RenameBuf), "%s", CB.TargetPath.filename().string().c_str());
                CB.ShowRename = true;
            }
            // Delete -> delete selection (multi allowed)
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !CB.Selection.empty()) {
                CB.DeleteList = CB.Selection;
                CB.ShowDeleteConfirm = true;
            }
            // Ctrl+A -> select all
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
                SelectClear(CB);
                std::error_code ec;
                for (std::filesystem::directory_iterator it(CB.Current, ec);
                     !ec && it != std::filesystem::directory_iterator(); ++it) {
                    if (it->is_directory(ec) || it->is_regular_file(ec)) SelectAdd(CB, it->path());
                }
            }
            // Ctrl+C -> copy to clipboard
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
                CB.Clipboard = CB.Selection;
            }
            // Ctrl+V -> paste (duplicate into current)
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && !CB.Clipboard.empty()) {
                for (auto& src : CB.Clipboard) {
                    auto destDir = CB.Current;
                    auto baseName = src.filename().string();
                    auto dest = UniqueSibling(destDir, baseName);
                    std::error_code ec;
                    if (std::filesystem::is_directory(src, ec)) {
                        std::filesystem::create_directories(dest, ec);
                        CopyDirectoryContents(src, dest);
                    } else {
                        std::filesystem::create_directories(dest.parent_path(), ec);
                        std::filesystem::copy_file(src, dest, std::filesystem::copy_options::none, ec);
                        if (ec) { dest = UniqueSibling(destDir, baseName);
                            std::filesystem::copy_file(src, dest, std::filesystem::copy_options::none, ec);
                        }
                    }
                }
            }
        }

        // Blank-area context menu
        if (ImGui::BeginPopupContextWindow("CBBlankContext",
            ImGuiPopupFlags_MouseButtonRight |
            ImGuiPopupFlags_NoOpenOverItems |
            ImGuiPopupFlags_NoOpenOverExistingPopup))
        {
            if (ImGui::BeginMenu("Add")) {
                if (ImGui::MenuItem("Blueprint (.blueprint)")) { OpenNewItemDialog(S, "blueprint.graph", CB.Current); }
                if (ImGui::MenuItem("GameMode (.gamemode)"))   { OpenNewItemDialog(S, "asset.gamemode",  CB.Current); }
                if (ImGui::MenuItem("Data Asset (.asset)"))    { OpenNewItemDialog(S, "asset.data",      CB.Current); }
                ImGui::Separator();
                if (ImGui::MenuItem("C++ Class…")) {
                    std::memset(s_NewCppName, 0, sizeof(s_NewCppName));
                    std::memset(s_NewCppSub,  0, sizeof(s_NewCppSub));
                    s_NewCppBaseIdx = 0;
                    s_CppWizardErr.clear();
                    s_ShowNewCppClass = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        // Gather entries (folders first, then files)
        struct Entry { std::filesystem::directory_entry de; bool isDir; };
        std::vector<Entry> entries;
        {
            std::vector<std::filesystem::directory_entry> dirs, files;
            std::error_code ec;
            for (std::filesystem::directory_iterator it(CB.Current, ec);
                 !ec && it != std::filesystem::directory_iterator(); ++it)
            {
                const auto& de = *it;
                const bool isDir  = de.is_directory(ec);
                const bool isFile = de.is_regular_file(ec);
                if (!isDir && !isFile) continue;
                const std::string name = de.path().filename().string();
                if (!CB.Filter.empty() && name.find(CB.Filter) == std::string::npos) continue;
                if (isDir) dirs.push_back(de); else files.push_back(de);
            }
            auto byName = [](const auto& a, const auto& b){ return a.path().filename().string() < b.path().filename().string(); };
            std::sort(dirs.begin(),  dirs.end(),  byName);
            std::sort(files.begin(), files.end(), byName);
            entries.reserve(dirs.size() + files.size());
            for (auto& d : dirs)  entries.push_back({d, true});
            for (auto& f : files) entries.push_back({f, false});
        }

        // if selection anchor invalid, fix it
        if (CB.AnchorIndex >= (int)entries.size()) CB.AnchorIndex = -1;

        // Cell geometry
        CB.ItemRects.clear();
        const float cellSide = CB.ThumbnailSize + CB.Padding * 2.0f; // square
        const ImGuiStyle& style = ImGui::GetStyle();
        const float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        auto drawItem = [&](int idx, const Entry& e)
        {
            ImGui::PushID(e.de.path().string().c_str());
            ImGui::BeginGroup();

            // Reserve the cell and catch clicks
            ImGui::InvisibleButton("##cell", ImVec2(cellSide, cellSide),
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool hovered       = ImGui::IsItemHovered();
            const bool leftClicked   = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            const bool rightClicked  = ImGui::IsItemClicked(ImGuiMouseButton_Right);
            const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            ImVec2 cellMin = ImGui::GetItemRectMin();
            ImVec2 cellMax = ImGui::GetItemRectMax();

            // Store rect for marquee
            CB.ItemRects.push_back({e.de.path(), cellMin, cellMax, e.isDir});

            // RIGHT-CLICK: select (if needed) and open item popup on THIS cell
            if (rightClicked) {
                ImGuiIO& io = ImGui::GetIO();
                if (!io.KeyCtrl && !IsSelected(CB, e.de.path())) {
                    SelectSet(CB, e.de.path());
                    CB.AnchorIndex = idx;
                }
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem(e.isDir ? "Open" : "Open (default)")) {
                    if (e.isDir) { CB.Current = e.de.path(); SelectClear(CB); }
                    else         { Logf("Editor: request open (context default) '%s'", e.de.path().string().c_str()); S.P.Editors = true; OpenFileInEditor(S, e.de.path()); }
                }
                if (!e.isDir && ImGui::MenuItem("Open in Editor")) {
                    Logf("Editor: request open (context explicit) '%s'", e.de.path().string().c_str());
                    S.P.Editors = true;
                    OpenFileInEditor(S, e.de.path());
                }
                bool single = (CB.Selection.size() == 1);
                if (ImGui::MenuItem("Rename", nullptr, false, single)) {
                    CB.TargetPath = CB.Selection.front();
                    std::snprintf(CB.RenameBuf, sizeof(CB.RenameBuf), "%s",
                                  CB.TargetPath.filename().string().c_str());
                    CB.ShowRename = true;
                }
                if (ImGui::MenuItem("Delete", nullptr, false, !CB.Selection.empty())) {
                    CB.DeleteList = CB.Selection; CB.ShowDeleteConfirm = true;
                }
#ifdef _WIN32
                if (single && ImGui::MenuItem("Reveal in Explorer")) {
                    RevealInExplorer(CB.Selection.front());
                }
#endif
                // Add → (only for folders makes sense, but we show here too for convenience)
                if (ImGui::BeginMenu("Add")) {
                    // If right-click was on a folder, we can route asset creation there; otherwise use CB.Current
                    std::filesystem::path targetFolder = e.isDir ? e.de.path() : CB.Current;
                    if (ImGui::MenuItem("Blueprint (.blueprint)")) { OpenNewItemDialog(S, "blueprint.graph", targetFolder); }
                    if (ImGui::MenuItem("GameMode (.gamemode)"))   { OpenNewItemDialog(S, "asset.gamemode",  targetFolder); }
                    if (ImGui::MenuItem("Data Asset (.asset)"))    { OpenNewItemDialog(S, "asset.data",      targetFolder); }
                    ImGui::Separator();
                    if (ImGui::MenuItem("C++ Class…")) {
                        std::memset(s_NewCppName, 0, sizeof(s_NewCppName));
                        std::memset(s_NewCppSub,  0, sizeof(s_NewCppSub));
                        s_NewCppBaseIdx = 0;
                        s_CppWizardErr.clear();
                        s_ShowNewCppClass = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            // Icon
            ImVec2 icon0 = cellMin + ImVec2(CB.Padding, CB.Padding);
            ImVec2 icon1 = icon0   + ImVec2(CB.ThumbnailSize, CB.ThumbnailSize);
            const bool selected = IsSelected(CB, e.de.path());
            DrawItemIcon(ImGui::GetWindowDrawList(), icon0, icon1, e.isDir, selected);

            // Label
            const std::string rawName = e.de.path().filename().string();
            const float labelAvail = cellSide - CB.Padding*2.0f;
            int approxMaxChars = (int)((labelAvail / ImGui::GetFontSize()) * 1.9f);
            std::string label = TruncMiddle(rawName, std::max(6, approxMaxChars));
            ImVec2 textPos = cellMin + ImVec2((cellSide - ImGui::CalcTextSize(label.c_str()).x) * 0.5f,
                                              CB.Padding + CB.ThumbnailSize + 4.0f);
            ImGui::SetCursorScreenPos(textPos);
            ImGui::TextUnformatted(label.c_str());

            // Selection / open behavior
            if (leftClicked) {
                ImGuiIO& io = ImGui::GetIO();
                bool ctrl  = io.KeyCtrl;
                bool shift = io.KeyShift;

                if (shift && !entries.empty()) {
                    if (CB.AnchorIndex < 0) CB.AnchorIndex = idx;
                    int a = std::min(CB.AnchorIndex, idx);
                    int b = std::max(CB.AnchorIndex, idx);
                    SelectClear(CB);
                    for (int i=a;i<=b;++i) SelectAdd(CB, entries[i].de.path());
                } else if (ctrl) {
                    if (IsSelected(CB, e.de.path())) SelectRemove(CB, e.de.path());
                    else SelectAdd(CB, e.de.path());
                    CB.AnchorIndex = idx;
                } else {
                    SelectSet(CB, e.de.path());
                    CB.AnchorIndex = idx;
                }
            }
            if (doubleClicked) {
                if (e.isDir) { CB.Current = e.de.path(); SelectClear(CB); }
                else         { Logf("Editor: request open (double-click) '%s'", e.de.path().string().c_str()); S.P.Editors = true; OpenFileInEditor(S, e.de.path()); }
            }

            // Drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                std::string payload;
                size_t count = 0;
                if (IsSelected(CB, e.de.path()) && !CB.Selection.empty()) {
                    for (auto& p : CB.Selection) { payload += p.string(); payload.push_back('\n'); ++count; }
                } else {
                    payload += e.de.path().string(); payload.push_back('\n'); count = 1;
                }
                ImGui::SetDragDropPayload("ACE_PATHS", payload.data(), (int)payload.size());
                if (count == 1) ImGui::TextUnformatted(e.de.path().filename().string().c_str());
                else ImGui::Text("%zu items", count);
                ImGui::EndDragDropSource();
            }
            // Drop target (folders accept drops -> move into folder)
            if (e.isDir && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* pld = ImGui::AcceptDragDropPayload("ACE_PATHS")) {
                    const char* data = static_cast<const char*>(pld->Data);
                    std::string all(data, data + pld->DataSize);
                    std::stringstream ss(all);
                    std::string line;
                    bool anyErr = false;
                    size_t moved = 0;

                    Logf("Drop onto GRID folder '%s' payloadSize=%d",
                         e.de.path().string().c_str(), (int)pld->DataSize);

                    while (std::getline(ss, line)) {
                        if (line.empty()) continue;
                        std::filesystem::path src(line);
                        std::string err;
                        Logf("  Move request: '%s' -> '%s'", src.string().c_str(), e.de.path().string().c_str());
                        if (!MoveEntryToDir(src, e.de.path(), err)) {
                            anyErr = true;
                            if (!err.empty()) S.CB.Error = err;
                            Logf("  Move FAILED: %s", err.c_str());
                        } else {
                            ++moved;
                            Logf("  Move OK");
                        }
                    }
                    if (!anyErr) S.CB.Error.clear();
                    SelectClear(S.CB);
                    Logf("Drop onto GRID folder '%s' done. moved=%zu, anyErr=%d",
                         e.de.path().string().c_str(), moved, (int)anyErr);
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::EndGroup();
            ImGui::PopID();
        };

        // Flow layout
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 16));
        for (int i = 0; i < (int)entries.size(); ++i) {
            drawItem(i, entries[i]);
            float lastX = ImGui::GetItemRectMax().x;
            float nextX = lastX + style.ItemSpacing.x + CB.ThumbnailSize + CB.Padding*2.0f;
            if (i + 1 < (int)entries.size() && nextX < windowVisibleX2)
                ImGui::SameLine();
        }
        ImGui::PopStyleVar();

        // Marquee selection (click-drag blank area)
        if (!ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.KeyCtrl) SelectClear(CB);
            CB.DragSelecting = true;
            CB.DragStart = CB.DragCur = ImGui::GetMousePos();
        }
        if (CB.DragSelecting) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                CB.DragCur = ImGui::GetMousePos();
                ImVec2 a = CB.DragStart, b = CB.DragCur;
                ImVec2 min(ImMin(a.x,b.x), ImMin(a.y,b.y));
                ImVec2 max(ImMax(a.x,b.x), ImMax(a.y,b.y));
                auto* dl2 = ImGui::GetWindowDrawList();
                dl2->AddRectFilled(min, max, IM_COL32(100, 150, 240, 40));
                dl2->AddRect(min, max, IM_COL32(100, 150, 240, 180), 0.0f, 0, 2.0f);
                for (auto& r : CB.ItemRects) {
                    bool overlap = !(r.max.x < min.x || r.min.x > max.x || r.max.y < min.y || r.min.y > max.y);
                    if (overlap) SelectAdd(CB, r.path);
                }
            } else {
                CB.DragSelecting = false;
            }
        }

        // Error line
        if (!CB.Error.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", CB.Error.c_str());
        }

        // ---- Modals (create/rename/delete) ----
        // Existing new-folder / text / custom / rename / delete modals unchanged...
        // (keep your existing code here exactly as you already have it)

        // ----- New C++ Class modal (Wizard) -----
        if (s_ShowNewCppClass) ImGui::OpenPopup("New C++ Class");
        if (ImGui::BeginPopupModal("New C++ Class", &s_ShowNewCppClass, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Create a new C++ class under /Source");
            ImGui::Separator();
            ImGui::InputText("Class Name", s_NewCppName, IM_ARRAYSIZE(s_NewCppName));

            // Base dropdown
            if (ImGui::BeginCombo("Base Class", G_CppBases[s_NewCppBaseIdx].label)) {
                for (int i=0; i<(int)(sizeof(G_CppBases)/sizeof(G_CppBases[0])); ++i) {
                    bool selected = (i == s_NewCppBaseIdx);
                    if (ImGui::Selectable(G_CppBases[i].label, selected)) s_NewCppBaseIdx = i;
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::InputText("Subfolder (optional, under /Source)", s_NewCppSub, IM_ARRAYSIZE(s_NewCppSub));
            if (!s_CppWizardErr.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1,0.35f,0.35f,1), "%s", s_CppWizardErr.c_str());
            }

            ImGui::Spacing();
            bool create = ImGui::Button("Create"); ImGui::SameLine();
            bool cancel = ImGui::Button("Cancel");

            if (create) {
                std::string cls = s_NewCppName;
                std::string sub = s_NewCppSub;
                // Normalize subfolder slashes and strip leading separators
                for (auto& ch : sub) if (ch=='\\') ch='/';
                while (!sub.empty() && (sub.front()=='/' || sub.front()=='.')) sub.erase(sub.begin());

                if (!IsValidCppIdentifier(cls)) {
                    s_CppWizardErr = "Invalid class name. Use letters, digits, and underscore; must start with letter/_";
                } else {
                    std::filesystem::path createdHeader;
                    if (!DoCreateNewCppClass(S, cls, s_NewCppBaseIdx, sub, createdHeader)) {
                        s_CppWizardErr = "Failed to create files (already exist? wrong Source path? see log).";
                    } else {
                        s_CppWizardErr.clear();
                        s_ShowNewCppClass = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            if (cancel) {
                s_CppWizardErr.clear();
                s_ShowNewCppClass = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }

    ImGui::EndChild(); // right
    ImGui::EndChild(); // split

    // Persist last folder (per session)
    S.CB.LastFolder = S.CB.Current;
    SaveSettings(S);

    ImGui::End();
}







// ----- Other Panels -----

static void DrawPanel_Viewport(EditorState& S) {
    if (ImGui::Begin("Viewport")) {
        ImGui::TextUnformatted("Game Viewport (stub)");
        if (!S.OpenMapPath.empty()) {
            ImGui::Text("Open Map: %s %s", S.OpenMapPath.filename().string().c_str(),
                        S.MapDirty ? "(modified)" : "");
        } else {
            ImGui::TextUnformatted("Open Map: (none)");
        }
        ImGui::Separator();
        ImGui::BulletText("This build shows a stub view. Next steps: grid, picking, gizmo.");
        ImGui::Dummy(ImVec2(0, 400));
    }
    ImGui::End();
}


static void DrawPanel_WorldOutliner(EditorState& S) {
    if (!ImGui::Begin("World Outliner")) { ImGui::End(); return; }

    if (!S.Project) {
        ImGui::TextUnformatted("No project loaded.");
        ImGui::End(); return;
    }

    // Header
    ImGui::TextUnformatted("World");
    ImGui::SameLine();
    if (S.MapDirty) ImGui::TextColored(ImVec4(1,0.6f,0.2f,1), "*");
    ImGui::Separator();

    // Buttons
    if (ImGui::Button("New Map"))       { NewEmptyMap(S); }
    ImGui::SameLine();
    if (ImGui::Button("Open Map..."))   { OpenMap(S); }
    ImGui::SameLine();
    if (ImGui::Button("Save"))          { if (SaveMap(S)) Logf("Map saved: %s", S.OpenMapPath.string().c_str()); }
    ImGui::SameLine();
    if (ImGui::Button("Save As..."))    { if (SaveMapAs(S)) Logf("Map saved as: %s", S.OpenMapPath.string().c_str()); }

    ImGui::Separator();
    if (ImGui::Button("+ Add Empty Actor")) {
        int idx = WorldAddEntity(S.EditorWorld, "EmptyActor");
        S.SelectedEntity = idx;
        S.MapDirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("- Delete") && S.SelectedEntity >= 0 && S.SelectedEntity < (int)S.EditorWorld.Entities.size()) {
        S.EditorWorld.Entities.erase(S.EditorWorld.Entities.begin() + S.SelectedEntity);
        if (S.SelectedEntity >= (int)S.EditorWorld.Entities.size()) S.SelectedEntity = (int)S.EditorWorld.Entities.size()-1;
        S.MapDirty = true;
    }

    ImGui::Separator();

    // List entities
    for (int i=0;i<(int)S.EditorWorld.Entities.size();++i){
        auto& e = S.EditorWorld.Entities[i];
        ImGuiTreeNodeFlags f = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
        if (i == S.SelectedEntity) f |= ImGuiTreeNodeFlags_Selected;
        bool open = ImGui::TreeNodeEx((void*)(intptr_t)e.Id, f, "%s##%d", e.Name.c_str(), e.Id);
        (void)open;
        if (ImGui::IsItemClicked()) S.SelectedEntity = i;
    }

    ImGui::End();
}


static void DrawPanel_Inspector(EditorState& S) {
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }

    if (S.SelectedEntity < 0 || S.SelectedEntity >= (int)S.EditorWorld.Entities.size()){
        ImGui::TextUnformatted("Selected Actor: (none)");
        ImGui::End(); return;
    }

    EEntity& e = S.EditorWorld.Entities[S.SelectedEntity];

    // Name
    {
        char nameBuf[256]; std::snprintf(nameBuf, sizeof(nameBuf), "%s", e.Name.c_str());
        if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) {
            e.Name = nameBuf;
            S.MapDirty = true;
        }
    }
    ImGui::Separator();

    // Transform
    ImGui::TextUnformatted("Transform");
    float pos[3] = { e.Xf.position.x, e.Xf.position.y, e.Xf.position.z };
    float rot[3] = { e.Xf.rotation.x, e.Xf.rotation.y, e.Xf.rotation.z };
    float scl[3] = { e.Xf.scale.x,    e.Xf.scale.y,    e.Xf.scale.z    };

    if (ImGui::DragFloat3("Position", pos, 0.1f)) { e.Xf.position = EV3(pos[0],pos[1],pos[2]); S.MapDirty = true; }
    if (ImGui::DragFloat3("Rotation", rot, 0.5f)) { e.Xf.rotation = EV3(rot[0],rot[1],rot[2]); S.MapDirty = true; }
    if (ImGui::DragFloat3("Scale",    scl, 0.01f)) {
        e.Xf.scale = EV3(std::max(0.0001f,scl[0]), std::max(0.0001f,scl[1]), std::max(0.0001f,scl[2]));
        S.MapDirty = true;
    }

    ImGui::Separator();

    // Components
    ImGui::TextUnformatted("Components");
    if (ImGui::Button("+ Static Mesh Component")) {
        EComponent c; c.Type = EComponentType::StaticMesh;
        e.Components.push_back(std::move(c));
        S.MapDirty = true;
    }
    ImGui::Spacing();

    for (int i=0;i<(int)e.Components.size(); ++i){
        auto& c = e.Components[i];
        if (c.Type == EComponentType::StaticMesh){
            if (ImGui::TreeNodeEx((void*)(intptr_t)(1000 + i), ImGuiTreeNodeFlags_DefaultOpen, "StaticMeshComponent #%d", i)){
                char meshBuf[512] = {}; std::snprintf(meshBuf, sizeof(meshBuf), "%s", c.StaticMesh.Mesh.generic_string().c_str());
                char matBuf [512] = {}; std::snprintf(matBuf,  sizeof(matBuf),  "%s", c.StaticMesh.Material.generic_string().c_str());
                if (ImGui::InputText("Mesh", meshBuf, IM_ARRAYSIZE(meshBuf))) {
                    c.StaticMesh.Mesh = std::filesystem::path(meshBuf);
                    S.MapDirty = true;
                }
                if (ImGui::InputText("Material", matBuf, IM_ARRAYSIZE(matBuf))) {
                    c.StaticMesh.Material = std::filesystem::path(matBuf);
                    S.MapDirty = true;
                }
                if (ImGui::Button("Remove Component")) {
                    e.Components.erase(e.Components.begin()+i);
                    S.MapDirty = true;
                    ImGui::TreePop();
                    break;
                }
                ImGui::TreePop();
            }
        }
    }

    ImGui::End();
}


static void DrawPanel_Console(EditorState&) {
    if (ImGui::Begin("Console")) {
        ImGui::TextWrapped("Welcome to ACE Editor.");
        ImGui::Separator();
        ImGui::TextUnformatted("Log (stub)");
        ImGui::BulletText("Loaded ImGui docking layout.");
        ImGui::BulletText("Initialized OpenGL2 backend.");
    }
    ImGui::End();
}
static void DrawPanel_Profiler(EditorState&) {
    if (ImGui::Begin("Profiler")) {
        ImGui::TextUnformatted("Profiler (stub)"); ImGui::Separator();
        ImGui::BulletText("Frame time graph"); ImGui::BulletText("CPU/GPU scopes"); ImGui::BulletText("Counters");
    }
    ImGui::End();
}
static void DrawPanel_BuildOutput(EditorState& S) {
    if (ImGui::Begin("Build Output", &S.P.BuildOutput)) {
        // --- Current selection summary ---
        ImGui::TextDisabled("Selection:");
        ImGui::BulletText("Config: %s",   ToStr(S.BuildSel.Config));
        ImGui::BulletText("Target: %s",   ToStr(S.BuildSel.Target));
        ImGui::BulletText("Platform: %s", ToStr(S.BuildSel.Platform));
        ImGui::BulletText("Jobs: %d",     S.BuildSel.ParallelJobs);
        if (!S.BuildSel.ExtraArgs.empty())
            ImGui::BulletText("Extra: %s", S.BuildSel.ExtraArgs.c_str());

        // --- Editable controls (optional) ---
        ImGui::Separator();
        int maxHW = std::max(1, (int)std::thread::hardware_concurrency());
        ImGui::SliderInt("Parallel Jobs", &S.BuildSel.ParallelJobs, 1, maxHW);
        ImGui::Checkbox("Open folder on success", &S.BuildSel.bOpenOutputOnSuccess);
        ImGui::InputText("Extra Args", &S.BuildSel.ExtraArgs);

        ImGui::Separator();

        // --- Controls & status ---
        if (S.Build.IsRunning) {
            const char* step = (S.Build.Step.empty() ? "Running..." : S.Build.Step.c_str());
            ImGui::TextColored(ImVec4(0.9f,0.9f,0.4f,1), "Status: %s", step);

            // Progress (fill the width)
            ImGui::ProgressBar(std::clamp(S.Build.Progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));

            // Disable build actions while running; only Cancel is active
            ImGui::BeginDisabled();
            ImGui::Button("Build"); ImGui::SameLine();
            ImGui::Button("Rebuild"); ImGui::SameLine();
            ImGui::Button("Clean");
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                CancelBuild(S);
            }

        } else {
            const char* st = (!S.Build.Step.empty() ? S.Build.Step.c_str() : "Idle");
            ImGui::Text("Status: %s", st);

            // Active buttons when idle
            if (ImGui::Button("Build")) {
                StartBuild(S, /*rebuild=*/false, /*clean=*/false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Rebuild")) {
                StartBuild(S, /*rebuild=*/true,  /*clean=*/false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clean")) {
                StartBuild(S, /*rebuild=*/false, /*clean=*/true);
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Compiler messages:");
        // TODO: stream your real build logs here into a scrolling child window, e.g.:
        // ImGui::BeginChild("BuildLog", ImVec2(0, 200), true);
        // for (const auto& line : S.Build.LogLines) ImGui::TextUnformatted(line.c_str());
        // ImGui::EndChild();
    }
    ImGui::End();
}


static void DrawPanel_PlayControls(EditorState&) {
    if (ImGui::Begin("Play Controls")) {
        if (ImGui::Button("Play")){} ImGui::SameLine();
        if (ImGui::Button("Pause")){} ImGui::SameLine();
        if (ImGui::Button("Stop")){}  ImGui::Separator();
        ImGui::TextUnformatted("Mode: Editor");
    }
    ImGui::End();
}

// ----- Dockspace & Layout -----

#if ACE_USE_IMGUI_DOCKING
static void ApplyDefaultLayout(ImGuiID dockspace_id) {
    using namespace ImGui;
    DockBuilderRemoveNode(dockspace_id);
    DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    DockBuilderSetNodeSize(dockspace_id, GetMainViewport()->Size);

    ImGuiID dock_main_id  = dockspace_id;
    ImGuiID dock_id_down  = DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);
    ImGuiID dock_id_left  = DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.28f, nullptr, &dock_main_id);
    ImGuiID dock_id_bottom_left = DockBuilderSplitNode(dock_id_down, ImGuiDir_Left, 0.5f, nullptr, &dock_id_down);

    DockBuilderDockWindow("Viewport",         dock_main_id);
    DockBuilderDockWindow("World Outliner",   dock_id_left);
    DockBuilderDockWindow("Play Controls",    dock_id_left);
    DockBuilderDockWindow("Inspector",        dock_id_right);
    DockBuilderDockWindow("Editors",          dock_id_right);
    DockBuilderDockWindow("Content Browser",  dock_id_bottom_left);
    DockBuilderDockWindow("Console",          dock_id_down);
    DockBuilderDockWindow("Profiler",         dock_id_down);
    DockBuilderDockWindow("Build Output",     dock_id_down);

    DockBuilderFinish(dockspace_id);
}
static void DrawDockspace(EditorState& S) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("ACEEditorDockspace", nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGuiID dockspace_id = ImGui::GetID("ACEEditorDockspaceID");
    ImGui::DockSpace(dockspace_id, ImVec2(0,0), ImGuiDockNodeFlags_PassthruCentralNode);
    static bool first = true;
    if (first || S.ResetLayoutRequested) { ApplyDefaultLayout(dockspace_id); first = false; S.ResetLayoutRequested = false; }
    ImGui::End();
}
#else
static void DrawDockspace(EditorState&) {}
#endif

// ----- Menus + Frame -----

// EditorUI_Menus.cpp (excerpt) — drop-in replacement for DrawMenus

static void StartBuild(EditorState& S, bool bRebuild, bool bClean) {
    // In the future, spawn your real build process here.
    S.Build.IsRunning = true;
    S.Build.CancelRequested = false;
    S.Build.Progress = 0.0f;
    S.Build.Step = bClean ? "Cleaning..." : (bRebuild ? "Rebuilding..." : "Building...");
    S.P.BuildOutput = true;

    Logf("Build started: Config=%s, Target=%s, Platform=%s, Rebuild=%d, Clean=%d, Jobs=%d, Extra='%s'",
         ToStr(S.BuildSel.Config), ToStr(S.BuildSel.Target), ToStr(S.BuildSel.Platform),
         (int)bRebuild, (int)bClean, S.BuildSel.ParallelJobs, S.BuildSel.ExtraArgs.c_str());
}

static void CancelBuild(EditorState& S) {
    if (!S.Build.IsRunning) return;
    S.Build.CancelRequested = true;
    S.Build.IsRunning = false;
    S.Build.Step = "Canceled";
    Logf("Build canceled by user.");
}


static void DrawMenus(EditorState& S) {
    if (!ImGui::BeginMainMenuBar()) return;

    // --- File ---
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project...")) {
            if (auto folder = PickFolderDialog()) {
                std::filesystem::path aceproj;
                if (NewProjectFromTemplate_Basic2D(*folder, aceproj)) {
                    S.ProjectFile = aceproj; S.Project = ace::Project::Load(S.ProjectFile);
                    if (S.Project) AddRecentProject(S, S.ProjectFile);
                    if (!S.Project) std::printf("[ACEEditor] Failed to load new project: %s\n", S.ProjectFile.string().c_str());
                }
            }
        }
        if (ImGui::MenuItem("Open Project...")) {
            if (auto sel = OpenAceprojDialog()) {
                S.ProjectFile = *sel; S.Project = ace::Project::Load(S.ProjectFile);
                if (S.Project) AddRecentProject(S, S.ProjectFile);
                if (!S.Project) std::printf("[ACEEditor] Failed to load: %s\n", S.ProjectFile.string().c_str());
            }
        }
        if (ImGui::BeginMenu("Open Recent")) {
            bool any = false;
            for (auto& p : S.Recent) {
                any = true;
                if (ImGui::MenuItem(p.string().c_str())) {
                    S.ProjectFile = p; S.Project = ace::Project::Load(S.ProjectFile);
                    if (S.Project) AddRecentProject(S, S.ProjectFile);
                }
            }
            if (!any) ImGui::MenuItem("(empty)", nullptr, false, false);
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Recent", nullptr, false, any)) { S.Recent.clear(); SaveSettings(S); }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save Project", nullptr, false, S.Project.has_value())) {
            if (S.SettingsDirty) {
                if (SaveProjectFileWithOverrides(S,
                    S.NameBuf.empty() ? S.Project->GetInfo().Name : S.NameBuf,
                    S.VersionBuf.empty() ? S.Project->GetInfo().EngineVersion : S.VersionBuf))
                { S.Project = ace::Project::Load(S.ProjectFile); S.SettingsDirty = false; }
            } else { S.Project->Save(S.ProjectFile); }
        }
        if (ImGui::MenuItem("Project Settings...", nullptr, false, S.Project.has_value())) { OpenProjectSettings(S); }
    #ifdef _WIN32
        if (ImGui::MenuItem("Open Project Folder", nullptr, false, S.Project.has_value())) { RevealInExplorer(S.ProjectFile); }
    #endif

        // ------- NEW: Map section -------
        ImGui::SeparatorText("Map");
        const bool hasProj = S.Project.has_value();
        if (ImGui::MenuItem("New Map", nullptr, false, hasProj)) { NewEmptyMap(S); }
        if (ImGui::MenuItem("Open Map...", nullptr, false, hasProj)) {
            if (!OpenMap(S)) std::printf("[ACEEditor] Open map canceled or failed\n");
        }
        if (ImGui::MenuItem("Save Map", "Ctrl+S", false, hasProj)) { SaveMap(S); }
        if (ImGui::MenuItem("Save Map As...", nullptr, false, hasProj)) { SaveMapAs(S); }

        ImGui::Separator();
        ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }

    // --- Edit (UE-like) ---
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Project Settings...", nullptr, false, S.Project.has_value())) {
                OpenProjectSettings(S);
            }

            ImGui::SeparatorText("Project");
            ImGui::MenuItem("Input Mappings",            nullptr, Flag_Settings_Input(),        S.Project.has_value());
            ImGui::MenuItem("Rendering",                 nullptr, Flag_Settings_Rendering(),    S.Project.has_value());
            ImGui::MenuItem("Physics",                   nullptr, Flag_Settings_Physics(),      S.Project.has_value());
            ImGui::MenuItem("Audio",                     nullptr, Flag_Settings_Audio(),        S.Project.has_value());
            ImGui::MenuItem("Scripting / Visual Graph",  nullptr, Flag_Settings_Scripting(),    S.Project.has_value());
            ImGui::MenuItem("Networking & Multiplayer",  nullptr, Flag_Settings_Networking(),   S.Project.has_value());
            ImGui::MenuItem("Online Services",           nullptr, Flag_Settings_Online(),       S.Project.has_value());
            ImGui::MenuItem("Asset Manager",             nullptr, Flag_Settings_AssetManager(), S.Project.has_value());
            ImGui::MenuItem("Gameplay (Tags / Globals)", nullptr, Flag_Settings_Gameplay(),     S.Project.has_value());
            ImGui::MenuItem("Content Paths & Mounts",    nullptr, Flag_Settings_ContentPaths(), S.Project.has_value());
            ImGui::MenuItem("Localization",              nullptr, Flag_Settings_Localization(), S.Project.has_value());
            ImGui::MenuItem("Build & Packaging",         nullptr, Flag_Settings_Build(),        S.Project.has_value());
            ImGui::MenuItem("Platforms",                 nullptr, Flag_Settings_Platforms(),    S.Project.has_value());
            ImGui::MenuItem("GC & Memory",               nullptr, Flag_Settings_Memory(),       S.Project.has_value());
            ImGui::MenuItem("Console Variables (CVars)", nullptr, Flag_Settings_CVars(),        true);

            ImGui::SeparatorText("Editor");
            ImGui::MenuItem("Editor Preferences...",      nullptr, Flag_EditorPreferences());
            ImGui::MenuItem("Plugins...",                 nullptr, Flag_Plugins());
            ImGui::MenuItem("Source Control",             nullptr, Flag_Settings_SourceControl());
            ImGui::MenuItem("Theme & Layout",             nullptr, Flag_Settings_Appearance());
            ImGui::MenuItem("Keymap & Shortcuts",         nullptr, Flag_Settings_Keymap());
            ImGui::MenuItem("Diagnostics (Logs/Telemetry)", nullptr, Flag_Settings_Diagnostics());

            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    // --- Build (UE-like) ---
    if (ImGui::BeginMenu("Build")) {
        enum class BuildConfig { Debug, Development, Shipping };
        enum class BuildTarget { Game, Editor, Server };
        enum class BuildPlatform { Windows, Linux, Mac };

        static BuildConfig   sCfg     = BuildConfig::Development;
        static BuildTarget   sTarget  = BuildTarget::Game;
        static BuildPlatform sPlat    = BuildPlatform::Windows;
        const bool hasProj = S.Project.has_value();

        if (ImGui::MenuItem("Build Project", "Ctrl+B", false, hasProj)) { S.P.BuildOutput = true; /* hook StartBuild here */ }
        if (ImGui::MenuItem("Rebuild Project", "Ctrl+Shift+B", false, hasProj)) { S.P.BuildOutput = true; }
        if (ImGui::MenuItem("Clean Project", nullptr, false, hasProj)) { S.P.BuildOutput = true; }
        if (ImGui::MenuItem("Cancel Build", nullptr, false, false)) { /* CancelBuild */ }

        ImGui::SeparatorText("Configuration");
        if (ImGui::MenuItem("Debug",        nullptr, sCfg==BuildConfig::Debug, hasProj))        sCfg = BuildConfig::Debug;
        if (ImGui::MenuItem("Development",  nullptr, sCfg==BuildConfig::Development, hasProj))  sCfg = BuildConfig::Development;
        if (ImGui::MenuItem("Shipping",     nullptr, sCfg==BuildConfig::Shipping, hasProj))     sCfg = BuildConfig::Shipping;

        ImGui::SeparatorText("Target");
        if (ImGui::MenuItem("Game",   nullptr, sTarget==BuildTarget::Game,   hasProj)) sTarget = BuildTarget::Game;
        if (ImGui::MenuItem("Editor", nullptr, sTarget==BuildTarget::Editor, hasProj)) sTarget = BuildTarget::Editor;
        if (ImGui::MenuItem("Server", nullptr, sTarget==BuildTarget::Server, hasProj)) sTarget = BuildTarget::Server;

        ImGui::SeparatorText("Platform");
        if (ImGui::MenuItem("Windows", nullptr, sPlat==BuildPlatform::Windows, hasProj)) sPlat = BuildPlatform::Windows;
        if (ImGui::MenuItem("Linux",   nullptr, sPlat==BuildPlatform::Linux,   hasProj)) sPlat = BuildPlatform::Linux;
        if (ImGui::MenuItem("macOS",   nullptr, sPlat==BuildPlatform::Mac,     hasProj)) sPlat = BuildPlatform::Mac;

        ImGui::Separator();
    #ifdef _WIN32
        if (ImGui::MenuItem("Open Build Folder", nullptr, false, hasProj)) {
            if (hasProj) RevealInExplorer(S.ProjectFile.parent_path());
        }
    #endif
        ImGui::MenuItem("Show Build Output", nullptr, &S.P.BuildOutput);
        if (ImGui::MenuItem("Build & Packaging Settings...", nullptr, false, hasProj)) {
            *Flag_Settings_Build() = true;
        }
        ImGui::EndMenu();
    }

    // --- Window ---
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("ImGui Demo", nullptr, &S.ShowDemo);
        ImGui::Separator();
        ImGui::MenuItem("Viewport",         nullptr, &S.P.Viewport);
        ImGui::MenuItem("World Outliner",   nullptr, &S.P.WorldOutliner);
        ImGui::MenuItem("Inspector",        nullptr, &S.P.Inspector);
        ImGui::MenuItem("Editors",          nullptr, &S.P.Editors);
        ImGui::MenuItem("Content Browser",  nullptr, &S.P.ContentBrowser);
        ImGui::MenuItem("Console",          nullptr, &S.P.Console);
        ImGui::MenuItem("Profiler",         nullptr, &S.P.Profiler);
        ImGui::MenuItem("Build Output",     nullptr, &S.P.BuildOutput);
        ImGui::MenuItem("Play Controls",    nullptr, &S.P.PlayControls);
        ImGui::EndMenu();
    }
#if ACE_USE_IMGUI_DOCKING
    if (ImGui::BeginMenu("Layout")) {
        if (ImGui::MenuItem("Reset Default Layout")) { S.ResetLayoutRequested = true; }
        ImGui::EndMenu();
    }
#endif
    ImGui::EndMainMenuBar();
}



static void DrawPanels(EditorState& S) {
    if (S.P.Viewport)        DrawPanel_Viewport(S);
    if (S.P.WorldOutliner)   DrawPanel_WorldOutliner(S);
    if (S.P.Inspector)       DrawPanel_Inspector(S);
    if (S.P.Editors)         DrawPanel_Editors(S);
    if (S.P.ContentBrowser)  DrawPanel_ContentBrowser(S);
    if (S.P.Console)         DrawPanel_Console(S);
    if (S.P.Profiler)        DrawPanel_Profiler(S);
    if (S.P.BuildOutput)     DrawPanel_BuildOutput(S);
    if (S.P.PlayControls)    DrawPanel_PlayControls(S);

    // New: render all settings panels (flags live in EditorSettingsPanels.cpp)
    DrawAllSettingsPanels(S);

    // Keep your existing Project Settings draw
    DrawProjectSettings(S);

#ifndef IMGUI_DISABLE_DEMO_WINDOWS
    if (S.ShowDemo) ImGui::ShowDemoWindow(&S.ShowDemo);
#endif
}

// ---------- Main ----------

int main(int argc, char** argv) {
    EditorState S{}; LoadSettings(S);
    if (auto arg = ParseProjectArg(argc, argv)) {
        S.ProjectFile = *arg; S.Project = ace::Project::Load(S.ProjectFile);
        if (S.Project) AddRecentProject(S, S.ProjectFile);
        if (!S.Project) std::printf("[ACEEditor] Failed to load project: %s\n", S.ProjectFile.string().c_str());
    }

    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1600, 900, "ACE Editor", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 2; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
#if ACE_USE_IMGUI_DOCKING
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // keep multi-viewport disabled while docking inside main window
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        DrawDockspace(S);
        DrawMenus(S);
        DrawPanels(S);

        ImGui::Render();
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
