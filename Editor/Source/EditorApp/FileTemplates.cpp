#include "FileTemplates.h"
#include "EditorPaths.h"      // helper that knows ACE_SOURCE_DIR / Project dir
#include <nlohmann/json.hpp>          // nlohmann
#include <filesystem>
#include <fstream>
#include <regex>

using json = nlohmann::json;
static std::vector<FileTemplate> GTemplates;

static void LoadDir(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) return;
    for (auto& e : std::filesystem::recursive_directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        std::ifstream f(e.path()); if (!f) continue;
        json j; f >> j;
        FileTemplate T;
        T.Id          = j.value("id", "");
        T.DisplayName = j.value("displayName", T.Id);
        T.Category    = j.value("category", "Misc");
        T.Icon        = j.value("icon", "");
        if (j.contains("files")) {
            for (auto& jf : j["files"]) {
                TemplateFileDesc FD;
                FD.OutputPath   = jf.value("output", "");
                FD.TemplatePath = jf.value("template", "");
                T.Files.push_back(std::move(FD));
            }
        }
        if (j.contains("defaults")) {
            for (auto& [k, v] : j["defaults"].items()) T.Defaults[k] = v.get<std::string>();
        }
        if (j.contains("varsFromDialog")) {
            for (auto& v : j["varsFromDialog"]) T.VarsFromDialog.push_back(v.get<std::string>());
        }
        if (!T.Id.empty() && !T.Files.empty()) GTemplates.push_back(std::move(T));
    }
}

void FileTemplates::Reload() {
    GTemplates.clear();
    LoadDir(EditorPaths::BuiltinFileTemplates());  // <ACE>/Templates/FileTemplates
    LoadDir(EditorPaths::ProjectFileTemplates());  // <Project>/Templates/FileTemplates
}

const std::vector<FileTemplate>& FileTemplates::All() { return GTemplates; }

std::vector<FileTemplate> FileTemplates::InCategory(const std::string& cat) {
    std::vector<FileTemplate> r;
    for (auto& t : GTemplates) if (t.Category == cat) r.push_back(t);
    return r;
}

const FileTemplate* FileTemplates::FindById(const std::string& id) {
    for (auto& t : GTemplates) if (t.Id == id) return &t;
    return nullptr;
}

// simple {{Var}} substitution
static std::string ExpandText(std::string text, const std::unordered_map<std::string,std::string>& vars) {
    static std::regex rx(R"(\{\{([A-Za-z0-9_]+)\}\})");
    std::smatch m;
    std::string out;
    while (std::regex_search(text, m, rx)) {
        out += m.prefix().str();
        auto it = vars.find(m[1].str());
        out += (it != vars.end()) ? it->second : std::string{};
        text = m.suffix().str();
    }
    out += text;
    return out;
}

bool FileTemplates::Expand(const FileTemplate& T,
                           const std::unordered_map<std::string, std::string>& Vars,
                           const std::string& DestRoot,
                           std::vector<std::string>& OutCreatedFiles,
                           std::string& OutError) {
    try {
        for (auto& f : T.Files) {
            // Read template
            std::ifstream in(f.TemplatePath); if (!in) { OutError = "Missing template: " + f.TemplatePath; return false; }
            std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string contents = ExpandText(raw, Vars);
            // Expand output path
            std::string rel = ExpandText(f.OutputPath, Vars);
            std::filesystem::path outPath = std::filesystem::path(DestRoot) / rel;
            std::filesystem::create_directories(outPath.parent_path());
            std::ofstream out(outPath, std::ios::binary);
            out << contents;
            OutCreatedFiles.push_back(outPath.string());
        }
        return true;
    } catch (const std::exception& e) {
        OutError = e.what();
        return false;
    }
}
