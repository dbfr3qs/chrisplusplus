#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "common/diagnostic.h"
#include "ast/ast.h"
#include "sema/types.h"

namespace chris {
namespace lsp {

// Minimal JSON value type for LSP messages
class Json {
public:
    enum Kind { Null, Bool, Int, String, Array, Object };

    Json() : kind_(Null) {}
    Json(bool v) : kind_(Bool), boolVal_(v) {}
    Json(int v) : kind_(Int), intVal_(v) {}
    Json(int64_t v) : kind_(Int), intVal_(v) {}
    Json(const std::string& v) : kind_(String), strVal_(v) {}
    Json(const char* v) : kind_(String), strVal_(v) {}
    Json(std::vector<Json> v) : kind_(Array), arrVal_(std::move(v)) {}
    Json(std::initializer_list<std::pair<std::string, Json>> pairs) : kind_(Object) {
        for (auto& p : pairs) {
            objKeys_.push_back(p.first);
            objMap_[p.first] = p.second;
        }
    }

    static Json object() { Json j; j.kind_ = Object; return j; }
    static Json array() { Json j; j.kind_ = Array; return j; }
    static Json null() { return Json(); }

    void set(const std::string& key, Json value) {
        if (kind_ != Object) { kind_ = Object; }
        if (objMap_.find(key) == objMap_.end()) {
            objKeys_.push_back(key);
        }
        objMap_[key] = std::move(value);
    }

    void push(Json value) {
        if (kind_ != Array) { kind_ = Array; }
        arrVal_.push_back(std::move(value));
    }

    Kind type() const { return kind_; }
    bool isNull() const { return kind_ == Null; }

    bool boolValue() const { return boolVal_; }
    int64_t intValue() const { return intVal_; }
    const std::string& stringValue() const { return strVal_; }
    const std::vector<Json>& arrayValue() const { return arrVal_; }

    bool has(const std::string& key) const {
        return kind_ == Object && objMap_.find(key) != objMap_.end();
    }

    const Json& operator[](const std::string& key) const {
        static Json nullJson;
        if (kind_ != Object) return nullJson;
        auto it = objMap_.find(key);
        if (it == objMap_.end()) return nullJson;
        return it->second;
    }

    const Json& operator[](size_t index) const {
        static Json nullJson;
        if (kind_ != Array || index >= arrVal_.size()) return nullJson;
        return arrVal_[index];
    }

    std::string serialize() const;

    static Json parse(const std::string& input);

private:
    Kind kind_;
    bool boolVal_ = false;
    int64_t intVal_ = 0;
    std::string strVal_;
    std::vector<Json> arrVal_;
    std::vector<std::string> objKeys_; // insertion order
    std::unordered_map<std::string, Json> objMap_;

    static Json parseValue(const std::string& input, size_t& pos);
    static Json parseString(const std::string& input, size_t& pos);
    static Json parseNumber(const std::string& input, size_t& pos);
    static Json parseArray(const std::string& input, size_t& pos);
    static Json parseObject(const std::string& input, size_t& pos);
    static void skipWhitespace(const std::string& input, size_t& pos);
    static std::string escapeString(const std::string& s);
    static std::string parseRawString(const std::string& input, size_t& pos);
};

// Document state
struct Document {
    std::string uri;
    std::string content;
    int version = 0;
    Program program;
    bool parsed = false;
};

// Symbol info for go-to-definition
struct SymbolInfo {
    std::string name;
    std::string kind; // "function", "class", "variable", etc.
    std::string detail; // type info or signature
    SourceLocation definitionLocation;
};

class LspServer {
public:
    LspServer();
    void run();

private:
    // JSON-RPC transport
    std::string readMessage();
    void sendMessage(const Json& msg);
    void sendResponse(const Json& id, const Json& result);
    void sendError(const Json& id, int code, const std::string& message);
    void sendNotification(const std::string& method, const Json& params);

    // LSP handlers
    void handleInitialize(const Json& id, const Json& params);
    void handleShutdown(const Json& id);
    void handleTextDocumentDidOpen(const Json& params);
    void handleTextDocumentDidChange(const Json& params);
    void handleTextDocumentDidClose(const Json& params);
    void handleTextDocumentDidSave(const Json& params);
    void handleTextDocumentCompletion(const Json& id, const Json& params);
    void handleTextDocumentHover(const Json& id, const Json& params);
    void handleTextDocumentDefinition(const Json& id, const Json& params);
    void handleTextDocumentDocumentSymbol(const Json& id, const Json& params);

    // Analysis
    void analyzeDocument(const std::string& uri);
    void publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diagnostics);
    std::string uriToPath(const std::string& uri) const;
    std::string pathToUri(const std::string& path) const;

    // Symbol collection
    void collectSymbols(const Program& program, const std::string& file);

    // Find symbol at position
    SymbolInfo* findSymbolAt(const std::string& uri, int line, int character);
    std::vector<Json> getCompletionItems(const std::string& uri, int line, int character);

    // State
    std::unordered_map<std::string, Document> documents_;
    std::unordered_map<std::string, std::vector<SymbolInfo>> symbols_; // uri -> symbols
    bool initialized_ = false;
    bool shutdownRequested_ = false;
};

} // namespace lsp
} // namespace chris
