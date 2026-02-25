#include "common/diagnostic.h"
#include <iostream>
#include <sstream>

namespace chris {

std::string Diagnostic::severityToString(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Error:   return "error";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Info:    return "info";
    }
    return "unknown";
}

std::string Diagnostic::toString() const {
    std::ostringstream oss;
    oss << severityToString(severity) << "[" << code << "]: " << message << "\n";
    oss << "  --> " << location.toString() << "\n";
    if (!sourceLine.empty()) {
        oss << "   | " << sourceLine << "\n";
    }
    if (suggestion.has_value()) {
        oss << "   = suggestion: " << suggestion.value() << "\n";
    }
    return oss.str();
}

static std::string escapeJson(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c; break;
        }
    }
    return oss.str();
}

std::string Diagnostic::toJson() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"code\":\"" << escapeJson(code) << "\",";
    oss << "\"severity\":\"" << severityToString(severity) << "\",";
    oss << "\"message\":\"" << escapeJson(message) << "\",";
    oss << "\"file\":\"" << escapeJson(location.file) << "\",";
    oss << "\"line\":" << location.line << ",";
    oss << "\"column\":" << location.column << ",";
    oss << "\"source_line\":\"" << escapeJson(sourceLine) << "\"";
    if (suggestion.has_value()) {
        oss << ",\"suggestion\":\"" << escapeJson(suggestion.value()) << "\"";
    }
    oss << "}";
    return oss.str();
}

void DiagnosticEngine::report(Diagnostic diagnostic) {
    if (diagnostic.severity == DiagnosticSeverity::Error) {
        errorCount_++;
    } else if (diagnostic.severity == DiagnosticSeverity::Warning) {
        warningCount_++;
    }
    diagnostics_.push_back(std::move(diagnostic));
}

void DiagnosticEngine::error(const std::string& code, const std::string& message,
                             const SourceLocation& location, const std::string& sourceLine,
                             const std::optional<std::string>& suggestion) {
    report({code, DiagnosticSeverity::Error, message, location, sourceLine, suggestion});
}

void DiagnosticEngine::warning(const std::string& code, const std::string& message,
                               const SourceLocation& location, const std::string& sourceLine,
                               const std::optional<std::string>& suggestion) {
    report({code, DiagnosticSeverity::Warning, message, location, sourceLine, suggestion});
}

bool DiagnosticEngine::hasErrors() const {
    return errorCount_ > 0;
}

size_t DiagnosticEngine::errorCount() const {
    return errorCount_;
}

size_t DiagnosticEngine::warningCount() const {
    return warningCount_;
}

const std::vector<Diagnostic>& DiagnosticEngine::diagnostics() const {
    return diagnostics_;
}

void DiagnosticEngine::printAll(bool jsonOutput) const {
    if (jsonOutput) {
        std::cout << "[";
        for (size_t i = 0; i < diagnostics_.size(); i++) {
            if (i > 0) std::cout << ",";
            std::cout << diagnostics_[i].toJson();
        }
        std::cout << "]" << std::endl;
    } else {
        for (const auto& diag : diagnostics_) {
            std::cerr << diag.toString();
        }
        if (errorCount_ > 0 || warningCount_ > 0) {
            std::cerr << "\n";
            if (errorCount_ > 0) {
                std::cerr << errorCount_ << " error(s)";
            }
            if (warningCount_ > 0) {
                if (errorCount_ > 0) std::cerr << ", ";
                std::cerr << warningCount_ << " warning(s)";
            }
            std::cerr << " generated." << std::endl;
        }
    }
}

void DiagnosticEngine::clear() {
    diagnostics_.clear();
    errorCount_ = 0;
    warningCount_ = 0;
}

} // namespace chris
