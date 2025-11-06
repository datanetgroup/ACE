project(ACEEditorProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/External/imgui)

add_executable(ACEEditor
        Source/EditorApp/main.cpp
        Source/EditorApp/EditorSettingsPanel.cpp   # <-- added
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp       # <-- keep
)

target_include_directories(ACEEditor PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${IMGUI_DIR}/misc/cpp
        ${CMAKE_SOURCE_DIR}/Engine/Source
        ${CMAKE_SOURCE_DIR}/External/nlohmann_json
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/EditorApp  # <-- added (nice to have)
)

# Compile-time defs:
# - ACE_SOURCE_DIR lets the app find /Templates reliably
# - (optional) ACE_USE_IMGUI_DOCKING once you install ImGui's docking branch
# - Windows header hygiene
target_compile_definitions(ACEEditor PRIVATE
        ACE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        ACE_USE_IMGUI_DOCKING=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_link_libraries(ACEEditor PRIVATE
        ACERuntime
        glfw
        opengl32
        comdlg32      # Open File dialog
        ole32         # IFileDialog (folder picker)
        shell32
        uuid
)

# Ensure the Templates folder exists next to the built EXE (for New Project…)
add_custom_command(TARGET ACEEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ACEEditor>/Templates"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Templates"
        "$<TARGET_FILE_DIR:ACEEditor>/Templates"
        COMMENT "Copying Templates folder next to ACEEditor.exe"
)
