#include "common/source_file.h"
#include <fstream>
#include <sstream>

namespace chris {

SourceFile::SourceFile(const std::string& path) : path_(path) {}

bool SourceFile::load() {
    std::ifstream file(path_);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    content_ = oss.str();

    buildLineIndex();
    return true;
}

const std::string& SourceFile::path() const {
    return path_;
}

const std::string& SourceFile::content() const {
    return content_;
}

std::string SourceFile::getLine(uint32_t lineNumber) const {
    if (lineNumber == 0 || lineNumber > lineOffsets_.size()) {
        return "";
    }

    size_t start = lineOffsets_[lineNumber - 1];
    size_t end;
    if (lineNumber < lineOffsets_.size()) {
        end = lineOffsets_[lineNumber];
    } else {
        end = content_.size();
    }

    // Strip trailing newline
    while (end > start && (content_[end - 1] == '\n' || content_[end - 1] == '\r')) {
        end--;
    }

    return content_.substr(start, end - start);
}

size_t SourceFile::lineCount() const {
    return lineOffsets_.size();
}

void SourceFile::buildLineIndex() {
    lineOffsets_.clear();
    lineOffsets_.push_back(0);

    for (size_t i = 0; i < content_.size(); i++) {
        if (content_[i] == '\n') {
            lineOffsets_.push_back(i + 1);
        }
    }
}

} // namespace chris
