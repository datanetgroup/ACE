#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

struct TemplateFileDesc {
    std::string OutputPath;   // e.g. "Source/{{ClassName}}.h"
    std::string TemplatePath; // e.g. "<ACE>/Templates/FileTemplates/Cxx/ClassHeader.h.tmpl"
};

struct FileTemplate {
    std::string Id;           // "cxx.gamemode"
    std::string DisplayName;  // "C++ Class: GameMode"
    std::string Category;     // "C++ Class"
    std::string Icon;         // optional icon key
    std::vector<TemplateFileDesc> Files;
    std::unordered_map<std::string, std::string> Defaults;  // e.g. Namespace=Game
    std::vector<std::string> VarsFromDialog;                 // e.g. { "ClassName", "Namespace" }
};

namespace FileTemplates {
    void Reload(); // scan built-in + project dirs
    const std::vector<FileTemplate>& All();
    std::vector<FileTemplate> InCategory(const std::string& cat);
    const FileTemplate* FindById(const std::string& id);

    // Vars includes Name/Location and derived values (ClassName, IncludeGuard…)
    bool Expand(const FileTemplate& T,
                const std::unordered_map<std::string, std::string>& Vars,
                const std::string& DestRoot,
                std::vector<std::string>& OutCreatedFiles,
                std::string& OutError);
}
