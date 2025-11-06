#include <cstdio>
#include <optional>
#include <filesystem>
#include <string>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"

#include "Runtime/Project/Project.h"

static void SetupImGui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
}

static void ShutdownImGui()
{
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

static void DrawDockspace()
{
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
    ImGui::Begin("ACEEditorDockspace", nullptr, flags);
    ImGui::PopStyleVar(2);

    ImGui::DockSpace(ImGui::GetID("ACEEditorDockspaceID"),
                     ImVec2(0,0),
                     ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

static void DrawUI()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New Project...", nullptr, false, true);
            ImGui::MenuItem("Open Project...", nullptr, false, true);
            ImGui::Separator();
            ImGui::MenuItem("Exit");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            static bool show_demo = false;
            ImGui::MenuItem("ImGui Demo", nullptr, &show_demo);
            if (show_demo) ImGui::ShowDemoWindow(&show_demo);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::Begin("Scene Hierarchy"); ImGui::TextUnformatted("(Empty)"); ImGui::End();
    ImGui::Begin("Inspector");       ImGui::TextUnformatted("No selection"); ImGui::End();
    ImGui::Begin("Content");         ImGui::TextUnformatted("Content/"); ImGui::End();
    ImGui::Begin("Console");         ImGui::TextWrapped("Welcome to ACE Editor."); ImGui::End();
}

int main()
{
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1600, 900, "ACE Editor", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 2; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    SetupImGui(window);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        DrawDockspace();
        DrawUI();

        ImGui::Render();
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }

        glfwSwapBuffers(window);
    }

    ShutdownImGui();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
