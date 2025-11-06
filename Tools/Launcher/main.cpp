#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <windows.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

static std::filesystem::path EditorExe()
{
    return std::filesystem::current_path() / "ACEEditor.exe";
}

static bool CreateProject(const std::filesystem::path& dir, const std::string& name)
{
    std::filesystem::create_directories(dir / "Source");
    std::filesystem::create_directories(dir / "Content");
    json j{{"Name",name},{"EngineVersion","0.1.0"},{"Modules",json::array()},{"Plugins",json::array()}};
    std::ofstream out(dir / (name + ".aceproj"));
    if (!out) return false; out << j.dump(2); return true;
}

static bool LaunchEditor(const std::filesystem::path& projectFile)
{
    std::wstring editor = EditorExe().wstring();
    std::wstring cmd = L"\"" + editor + L"\" --project \"" + projectFile.wstring() + L"\"";
    STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return false;
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return true;
}

int main()
{
    std::cout << "ACE Launcher\n1) Create Project\n2) Open Project\n> ";
    int c=0; std::cin >> c; std::cin.ignore();
    if (c==1) {
        std::string pathStr,name;
        std::cout << "Project folder path: "; std::getline(std::cin, pathStr);
        std::cout << "Project name: ";        std::getline(std::cin, name);
        auto dir = std::filesystem::path(pathStr);
        if (!CreateProject(dir, name)) { std::cerr<<"Create failed\n"; return 1; }
        LaunchEditor(dir / (name + ".aceproj"));
    } else if (c==2) {
        std::string fileStr; std::cout << ".aceproj path: "; std::getline(std::cin, fileStr);
        LaunchEditor(fileStr);
    }
    return 0;
}
