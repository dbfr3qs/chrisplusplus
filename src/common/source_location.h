#pragma once

#include <string>
#include <cstdint>

namespace chris {

struct SourceLocation {
    std::string file;
    uint32_t line;
    uint32_t column;

    SourceLocation() : file(""), line(0), column(0) {}
    SourceLocation(const std::string& file, uint32_t line, uint32_t column)
        : file(file), line(line), column(column) {}

    std::string toString() const;
};

} // namespace chris
