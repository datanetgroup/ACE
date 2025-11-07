#pragma once

// --- Platform hygiene (match what you already use in CMake) ---
#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
#endif

// --- Common STL types you’ll want almost everywhere ---
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <filesystem>
#include "Runtime/Core/AceObjectMacros.h"

// --- Engine macro surface (ACE_CLASS/ACE_PROPERTY/etc.) ---
#include "Runtime/Core/AceObjectMacros.h"

// You can add engine-wide forward decls / logging macros here later.
