project(ACEEditorProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/External/imgui)
# ImGui TextEditor (third-party widget) lives at project-root/External/imguite
set(IMGUI_TE_DIR ${CMAKE_SOURCE_DIR}/External/imguite)

add_executable(ACEEditor
        Source/EditorApp/main.cpp
        Source/EditorApp/EditorSettingsPanel.cpp
        Source/EditorApp/EditorCodegen.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

# --- ImGui TextEditor (third-party) ---
# Ensure the files exist where we expect them
if(NOT EXISTS "${IMGUI_TE_DIR}/TextEditor.h")
    message(FATAL_ERROR "Missing TextEditor.h at ${IMGUI_TE_DIR}")
endif()
if(NOT EXISTS "${IMGUI_TE_DIR}/TextEditor.cpp")
    message(FATAL_ERROR "Missing TextEditor.cpp at ${IMGUI_TE_DIR}")
endif()

target_sources(ACEEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Themes/ThemeManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Themes/GMS2/ThemeGMS2.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/EditorApp/EditorPreferences.cpp

        # Add the widget's implementation (.cpp) — do NOT add the header as a source
        ${IMGUI_TE_DIR}/TextEditor.cpp
)

target_include_directories(ACEEditor PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${IMGUI_DIR}/misc/cpp

        # Include path for TextEditor.h
        ${IMGUI_TE_DIR}

        ${CMAKE_SOURCE_DIR}/Engine/Source
        ${CMAKE_SOURCE_DIR}/External/nlohmann_json
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/EditorApp
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(ACEEditor PRIVATE
        ACE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        ACE_USE_IMGUI_DOCKING=1
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        IMGUI_DEFINE_MATH_OPERATORS
)

target_link_libraries(ACEEditor PRIVATE
        ACERuntime
        glfw
        opengl32
        comdlg32
        ole32
        shell32
        uuid
)

add_custom_command(TARGET ACEEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ACEEditor>/Templates"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Templates"
        "$<TARGET_FILE_DIR:ACEEditor>/Templates"
        COMMENT "Copying Templates folder next to ACEEditor.exe"
)
