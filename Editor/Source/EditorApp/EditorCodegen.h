#pragma once
#include <filesystem>

// Ensures <Class>.generated.h exists next to <Class>.h.
// Creates a minimal stub if missing so the project compiles today.
// Safe to call multiple times.
void WriteGeneratedStubIfMissing(const std::filesystem::path& headerPath);
