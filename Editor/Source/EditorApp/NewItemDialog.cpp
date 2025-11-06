#include "NewItemDialog.h"
#include "FileTemplates.h"
#include "imgui.h"

static bool s_Open = false;
static std::string s_TemplateId;
static std::string s_Name;
static std::string s_Location;

bool NewItemDialog::Draw(EditorState& /*S*/,
                         const std::string& preselectedTemplateId,
                         const std::string& initialName,
                         std::unordered_map<std::string, std::string>& OutVars,
                         std::string& OutChosenTemplateId,
                         bool& OutOk) {
    if (!s_Open) {
        s_Open = true;
        s_TemplateId = preselectedTemplateId;
        s_Name = initialName;
        s_Location = ""; // fill with current content folder
    }
    bool finished = false;
    if (ImGui::Begin("New Item", &s_Open, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Name
        ImGui::InputText("Name", &s_Name);
        // Template dropdown
        const FileTemplate* sel = FileTemplates::FindById(s_TemplateId);
        if (ImGui::BeginCombo("Template", sel ? sel->DisplayName.c_str() : s_TemplateId.c_str())) {
            for (auto& t : FileTemplates::All()) {
                bool isSel = (t.Id == s_TemplateId);
                if (ImGui::Selectable((t.DisplayName + "##" + t.Id).c_str(), isSel))
                    s_TemplateId = t.Id;
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        // TODO Location picker
        // Options generated from template
        if (sel) {
            for (auto& var : sel->VarsFromDialog) {
                // naive: only Namespace for now; extend as needed
                static std::string ns; // you can store per-var temp buffers
                if (var == "Namespace") ImGui::InputText("Namespace", &ns);
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Create")) {
            OutVars["Name"] = s_Name;
            OutVars["ClassName"] = s_Name; // or apply a transform
            OutVars["Namespace"] = "";     // fill from inputs/defaults
            OutChosenTemplateId = s_TemplateId;
            OutOk = true;
            finished = true;
            s_Open = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            OutOk = false;
            finished = true;
            s_Open = false;
        }
        ImGui::End();
    }
    return finished;
}
