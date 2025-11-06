#include "Runtime/Project/Project.h"
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ace {
    std::optional<Project> Project::Load(const std::filesystem::path& aceprojFile)
    {
        if (!std::filesystem::exists(aceprojFile)) return std::nullopt;
        std::ifstream in(aceprojFile);
        if (!in) return std::nullopt;

        json j; in >> j;

        Project p;
        p.Info.Name          = j.value("Name", "Untitled");
        p.Info.EngineVersion = j.value("EngineVersion", "0.1.0");
        p.Info.RootDir       = aceprojFile.parent_path();

        if (j.contains("Modules"))
            for (auto& m : j["Modules"]) p.Info.Modules.push_back(m.get<std::string>());
        if (j.contains("Plugins"))
            for (auto& m : j["Plugins"]) p.Info.Plugins.push_back(m.get<std::string>());
        return p;
    }

    bool Project::Save(const std::filesystem::path& aceprojFile) const
    {
        json j;
        j["Name"]          = Info.Name;
        j["EngineVersion"] = Info.EngineVersion;
        j["Modules"]       = Info.Modules;
        j["Plugins"]       = Info.Plugins;

        std::ofstream out(aceprojFile);
        if (!out) return false;
        out << j.dump(2);
        return true;
    }
}
