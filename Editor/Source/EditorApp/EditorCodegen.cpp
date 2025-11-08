#include "EditorCodegen.h"
#include <fstream>
#include <system_error>

void WriteGeneratedStubIfMissing(const std::filesystem::path& headerPath)
{
    try {
        const auto stem = headerPath.stem().string();            // "MyClass"
        const auto gen  = headerPath.parent_path() / (stem + ".generated.h");
        if (!std::filesystem::exists(gen)) {
            std::error_code ec;
            std::filesystem::create_directories(gen.parent_path(), ec);
            std::ofstream out(gen, std::ios::binary | std::ios::trunc);
            if (out) {
                out << "#pragma once\n"
                       "// Auto-generated stub by ACE Editor. Replaced by header tool later.\n";
            }
        }
    } catch (...) {
        // non-fatal convenience
    }
}
