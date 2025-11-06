project(ACELauncherProj LANGUAGES CXX)
add_executable(ACELauncher main.cpp)
target_include_directories(ACELauncher PRIVATE
        ${CMAKE_SOURCE_DIR}/External/nlohmann_json
)
