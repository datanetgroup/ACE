#pragma once
#include <string>
#include <unordered_map>

struct EditorState;

namespace NewItemDialog {
    // Returns true if dialog finished (Ok or Cancel). On Ok, OutVars filled.
    bool Draw(EditorState& S,
              const std::string& preselectedTemplateId,
              const std::string& initialName,
              std::unordered_map<std::string, std::string>& OutVars,
              std::string& OutChosenTemplateId,
              bool& OutOk);
}
