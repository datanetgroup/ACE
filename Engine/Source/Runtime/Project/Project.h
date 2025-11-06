#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace ace {
    struct ProjectInfo {
        std::string Name;
        std::string EngineVersion;
        std::filesystem::path RootDir;
        std::vector<std::string> Modules;
        std::vector<std::string> Plugins;
    };

    class Project {
    public:
        static std::optional<Project> Load(const std::filesystem::path& aceprojFile);
        bool Save(const std::filesystem::path& aceprojFile) const;

        const ProjectInfo& GetInfo() const { return Info; }
        std::filesystem::path ContentDir() const { return Info.RootDir / "Content"; }
        std::filesystem::path SourceDir()  const { return Info.RootDir / "Source";  }

    private:
        ProjectInfo Info;
    };
}
