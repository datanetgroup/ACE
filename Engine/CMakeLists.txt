project(ACERuntimeProj LANGUAGES CXX)

add_library(ACERuntime STATIC
        Source/Runtime/Project/Project.cpp
)

target_include_directories(ACERuntime PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_SOURCE_DIR}/External/nlohmann_json
)

target_compile_definitions(ACERuntime PUBLIC
        ACE_ENGINE_VERSION="0.1.0"
)
