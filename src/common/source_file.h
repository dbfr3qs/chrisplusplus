#pragma once

#include <string>
#include <vector>

namespace chris {

class SourceFile {
public:
    SourceFile(const std::string& path);

    bool load();
    const std::string& path() const;
    const std::string& content() const;
    std::string getLine(uint32_t lineNumber) const;
    size_t lineCount() const;

private:
    std::string path_;
    std::string content_;
    std::vector<size_t> lineOffsets_;

    void buildLineIndex();
};

} // namespace chris
