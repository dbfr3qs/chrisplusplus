#pragma once

#include <string>
#include <vector>
#include <optional>
#include "common/source_location.h"

namespace chris {

enum class DiagnosticSeverity {
    Error,
    Warning,
    Info
};

struct Diagnostic {
    std::string code;
    DiagnosticSeverity severity;
    std::string message;
    SourceLocation location;
    std::string sourceLine;
    std::optional<std::string> suggestion;

    std::string toString() const;
    std::string toJson() const;

    static std::string severityToString(DiagnosticSeverity severity);
};

class DiagnosticEngine {
public:
    void report(Diagnostic diagnostic);
    void error(const std::string& code, const std::string& message,
               const SourceLocation& location, const std::string& sourceLine = "",
               const std::optional<std::string>& suggestion = std::nullopt);
    void warning(const std::string& code, const std::string& message,
                 const SourceLocation& location, const std::string& sourceLine = "",
                 const std::optional<std::string>& suggestion = std::nullopt);

    bool hasErrors() const;
    size_t errorCount() const;
    size_t warningCount() const;
    const std::vector<Diagnostic>& diagnostics() const;

    void printAll(bool jsonOutput = false) const;
    void clear();

private:
    std::vector<Diagnostic> diagnostics_;
    size_t errorCount_ = 0;
    size_t warningCount_ = 0;
};

} // namespace chris
