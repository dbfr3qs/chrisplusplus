#include "lsp/lsp_server.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace chris {
namespace lsp {

// ============================================================================
// Json implementation
// ============================================================================

std::string Json::escapeString(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

std::string Json::serialize() const {
    switch (kind_) {
        case Null: return "null";
        case Bool: return boolVal_ ? "true" : "false";
        case Int: return std::to_string(intVal_);
        case String: return "\"" + escapeString(strVal_) + "\"";
        case Array: {
            std::string result = "[";
            for (size_t i = 0; i < arrVal_.size(); i++) {
                if (i > 0) result += ",";
                result += arrVal_[i].serialize();
            }
            result += "]";
            return result;
        }
        case Object: {
            std::string result = "{";
            bool first = true;
            for (auto& key : objKeys_) {
                if (!first) result += ",";
                first = false;
                result += "\"" + escapeString(key) + "\":" + objMap_.at(key).serialize();
            }
            result += "}";
            return result;
        }
    }
    return "null";
}

void Json::skipWhitespace(const std::string& input, size_t& pos) {
    while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\t' ||
           input[pos] == '\n' || input[pos] == '\r')) {
        pos++;
    }
}

std::string Json::parseRawString(const std::string& input, size_t& pos) {
    if (pos >= input.size() || input[pos] != '"') return "";
    pos++; // skip opening quote
    std::string result;
    while (pos < input.size() && input[pos] != '"') {
        if (input[pos] == '\\' && pos + 1 < input.size()) {
            pos++;
            switch (input[pos]) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case '/':  result += '/'; break;
                case 'u': {
                    // Basic unicode escape - just skip for now
                    pos += 4;
                    result += '?';
                    continue;
                }
                default: result += input[pos]; break;
            }
        } else {
            result += input[pos];
        }
        pos++;
    }
    if (pos < input.size()) pos++; // skip closing quote
    return result;
}

Json Json::parseString(const std::string& input, size_t& pos) {
    return Json(parseRawString(input, pos));
}

Json Json::parseNumber(const std::string& input, size_t& pos) {
    size_t start = pos;
    if (pos < input.size() && input[pos] == '-') pos++;
    while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') pos++;
    // For LSP we only need integers
    std::string numStr = input.substr(start, pos - start);
    return Json(static_cast<int64_t>(std::stoll(numStr)));
}

Json Json::parseArray(const std::string& input, size_t& pos) {
    pos++; // skip [
    Json arr = Json::array();
    skipWhitespace(input, pos);
    if (pos < input.size() && input[pos] == ']') { pos++; return arr; }
    while (pos < input.size()) {
        arr.push(parseValue(input, pos));
        skipWhitespace(input, pos);
        if (pos < input.size() && input[pos] == ',') { pos++; skipWhitespace(input, pos); }
        else break;
    }
    if (pos < input.size() && input[pos] == ']') pos++;
    return arr;
}

Json Json::parseObject(const std::string& input, size_t& pos) {
    pos++; // skip {
    Json obj = Json::object();
    skipWhitespace(input, pos);
    if (pos < input.size() && input[pos] == '}') { pos++; return obj; }
    while (pos < input.size()) {
        skipWhitespace(input, pos);
        std::string key = parseRawString(input, pos);
        skipWhitespace(input, pos);
        if (pos < input.size() && input[pos] == ':') pos++;
        skipWhitespace(input, pos);
        obj.set(key, parseValue(input, pos));
        skipWhitespace(input, pos);
        if (pos < input.size() && input[pos] == ',') { pos++; }
        else break;
    }
    if (pos < input.size() && input[pos] == '}') pos++;
    return obj;
}

Json Json::parseValue(const std::string& input, size_t& pos) {
    skipWhitespace(input, pos);
    if (pos >= input.size()) return Json::null();

    char c = input[pos];
    if (c == '"') return parseString(input, pos);
    if (c == '{') return parseObject(input, pos);
    if (c == '[') return parseArray(input, pos);
    if (c == 't') { pos += 4; return Json(true); }
    if (c == 'f') { pos += 5; return Json(false); }
    if (c == 'n') { pos += 4; return Json::null(); }
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(input, pos);
    return Json::null();
}

Json Json::parse(const std::string& input) {
    size_t pos = 0;
    return parseValue(input, pos);
}

// ============================================================================
// LspServer implementation
// ============================================================================

LspServer::LspServer() {}

std::string LspServer::readMessage() {
    // Read headers
    std::string headers;
    int contentLength = -1;

    while (true) {
        std::string line;
        while (true) {
            int c = std::cin.get();
            if (c == EOF || std::cin.eof()) return "";
            if (c == '\n') break;
            if (c != '\r') line += static_cast<char>(c);
        }
        if (line.empty()) break; // empty line = end of headers

        // Parse Content-Length
        if (line.substr(0, 16) == "Content-Length: ") {
            contentLength = std::stoi(line.substr(16));
        }
    }

    if (contentLength <= 0) return "";

    // Read body
    std::string body(contentLength, '\0');
    std::cin.read(&body[0], contentLength);
    if (std::cin.gcount() != contentLength) return "";

    return body;
}

void LspServer::sendMessage(const Json& msg) {
    std::string body = msg.serialize();
    std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
    std::cout << header << body;
    std::cout.flush();
}

void LspServer::sendResponse(const Json& id, const Json& result) {
    Json msg = Json::object();
    msg.set("jsonrpc", Json("2.0"));
    msg.set("id", id);
    msg.set("result", result);
    sendMessage(msg);
}

void LspServer::sendError(const Json& id, int code, const std::string& message) {
    Json error = Json::object();
    error.set("code", Json(code));
    error.set("message", Json(message));

    Json msg = Json::object();
    msg.set("jsonrpc", Json("2.0"));
    msg.set("id", id);
    msg.set("error", error);
    sendMessage(msg);
}

void LspServer::sendNotification(const std::string& method, const Json& params) {
    Json msg = Json::object();
    msg.set("jsonrpc", Json("2.0"));
    msg.set("method", Json(method));
    msg.set("params", params);
    sendMessage(msg);
}

void LspServer::run() {
    // Set stdin/stdout to binary mode
    std::cin >> std::noskipws;

    while (true) {
        std::string body = readMessage();
        if (body.empty()) {
            if (std::cin.eof()) break;
            continue;
        }

        Json msg = Json::parse(body);
        std::string method;
        if (msg.has("method")) {
            method = msg["method"].stringValue();
        }

        Json id = msg.has("id") ? msg["id"] : Json::null();
        Json params = msg.has("params") ? msg["params"] : Json::object();

        if (method == "initialize") {
            handleInitialize(id, params);
        } else if (method == "initialized") {
            // no-op
        } else if (method == "shutdown") {
            handleShutdown(id);
        } else if (method == "exit") {
            break;
        } else if (method == "textDocument/didOpen") {
            handleTextDocumentDidOpen(params);
        } else if (method == "textDocument/didChange") {
            handleTextDocumentDidChange(params);
        } else if (method == "textDocument/didClose") {
            handleTextDocumentDidClose(params);
        } else if (method == "textDocument/didSave") {
            handleTextDocumentDidSave(params);
        } else if (method == "textDocument/completion") {
            handleTextDocumentCompletion(id, params);
        } else if (method == "textDocument/hover") {
            handleTextDocumentHover(id, params);
        } else if (method == "textDocument/definition") {
            handleTextDocumentDefinition(id, params);
        } else if (method == "textDocument/documentSymbol") {
            handleTextDocumentDocumentSymbol(id, params);
        } else if (!id.isNull()) {
            // Unknown request — respond with method not found
            sendError(id, -32601, "Method not found: " + method);
        }
        // Unknown notifications are silently ignored
    }
}

// ============================================================================
// LSP handlers
// ============================================================================

void LspServer::handleInitialize(const Json& id, const Json& params) {
    Json capabilities = Json::object();

    // Text document sync: full content on open/change
    Json textDocSync = Json::object();
    textDocSync.set("openClose", Json(true));
    textDocSync.set("change", Json(1)); // 1 = Full sync
    textDocSync.set("save", Json(true));
    capabilities.set("textDocumentSync", textDocSync);

    // Completion
    Json completionProvider = Json::object();
    Json triggerChars = Json::array();
    triggerChars.push(Json("."));
    completionProvider.set("triggerCharacters", triggerChars);
    capabilities.set("completionProvider", completionProvider);

    // Hover
    capabilities.set("hoverProvider", Json(true));

    // Definition
    capabilities.set("definitionProvider", Json(true));

    // Document symbols
    capabilities.set("documentSymbolProvider", Json(true));

    Json result = Json::object();
    result.set("capabilities", capabilities);

    Json serverInfo = Json::object();
    serverInfo.set("name", Json("chrisplusplus-lsp"));
    serverInfo.set("version", Json("0.1.0"));
    result.set("serverInfo", serverInfo);

    sendResponse(id, result);
    initialized_ = true;
}

void LspServer::handleShutdown(const Json& id) {
    shutdownRequested_ = true;
    sendResponse(id, Json::null());
}

void LspServer::handleTextDocumentDidOpen(const Json& params) {
    auto& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].stringValue();
    std::string text = textDoc["text"].stringValue();
    int version = static_cast<int>(textDoc["version"].intValue());

    Document doc;
    doc.uri = uri;
    doc.content = text;
    doc.version = version;
    documents_[uri] = std::move(doc);

    analyzeDocument(uri);
}

void LspServer::handleTextDocumentDidChange(const Json& params) {
    auto& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].stringValue();
    int version = static_cast<int>(textDoc["version"].intValue());

    auto& changes = params["contentChanges"];
    if (changes.type() == Json::Array && changes.arrayValue().size() > 0) {
        // Full sync: take the last change
        auto& lastChange = changes[changes.arrayValue().size() - 1];
        documents_[uri].content = lastChange["text"].stringValue();
        documents_[uri].version = version;
    }

    analyzeDocument(uri);
}

void LspServer::handleTextDocumentDidClose(const Json& params) {
    std::string uri = params["textDocument"]["uri"].stringValue();
    documents_.erase(uri);
    symbols_.erase(uri);

    // Clear diagnostics
    Json diagParams = Json::object();
    diagParams.set("uri", Json(uri));
    diagParams.set("diagnostics", Json::array());
    sendNotification("textDocument/publishDiagnostics", diagParams);
}

void LspServer::handleTextDocumentDidSave(const Json& params) {
    std::string uri = params["textDocument"]["uri"].stringValue();
    analyzeDocument(uri);
}

void LspServer::handleTextDocumentCompletion(const Json& id, const Json& params) {
    std::string uri = params["textDocument"]["uri"].stringValue();
    int line = static_cast<int>(params["position"]["line"].intValue());
    int character = static_cast<int>(params["position"]["character"].intValue());

    auto items = getCompletionItems(uri, line, character);

    Json result = Json::array();
    for (auto& item : items) {
        result.push(std::move(item));
    }

    sendResponse(id, result);
}

void LspServer::handleTextDocumentHover(const Json& id, const Json& params) {
    std::string uri = params["textDocument"]["uri"].stringValue();
    int line = static_cast<int>(params["position"]["line"].intValue());
    int character = static_cast<int>(params["position"]["character"].intValue());

    auto* sym = findSymbolAt(uri, line + 1, character + 1); // LSP is 0-based, our locations are 1-based

    if (sym) {
        std::string hoverText = "**" + sym->kind + "** `" + sym->name + "`";
        if (!sym->detail.empty()) {
            hoverText += "\n\n" + sym->detail;
        }

        Json contents = Json::object();
        contents.set("kind", Json("markdown"));
        contents.set("value", Json(hoverText));

        Json result = Json::object();
        result.set("contents", contents);
        sendResponse(id, result);
    } else {
        sendResponse(id, Json::null());
    }
}

void LspServer::handleTextDocumentDefinition(const Json& id, const Json& params) {
    std::string uri = params["textDocument"]["uri"].stringValue();
    int line = static_cast<int>(params["position"]["line"].intValue());
    int character = static_cast<int>(params["position"]["character"].intValue());

    auto* sym = findSymbolAt(uri, line + 1, character + 1);

    if (sym && sym->definitionLocation.line > 0) {
        Json location = Json::object();
        location.set("uri", Json(pathToUri(sym->definitionLocation.file)));

        Json range = Json::object();
        Json start = Json::object();
        start.set("line", Json(static_cast<int64_t>(sym->definitionLocation.line - 1)));
        start.set("character", Json(static_cast<int64_t>(sym->definitionLocation.column - 1)));
        Json end = Json::object();
        end.set("line", Json(static_cast<int64_t>(sym->definitionLocation.line - 1)));
        end.set("character", Json(static_cast<int64_t>(sym->definitionLocation.column - 1)));
        range.set("start", start);
        range.set("end", end);
        location.set("range", range);

        sendResponse(id, location);
    } else {
        sendResponse(id, Json::null());
    }
}

void LspServer::handleTextDocumentDocumentSymbol(const Json& id, const Json& params) {
    std::string uri = params["textDocument"]["uri"].stringValue();

    Json result = Json::array();
    auto it = symbols_.find(uri);
    if (it != symbols_.end()) {
        for (auto& sym : it->second) {
            Json symbol = Json::object();
            symbol.set("name", Json(sym.name));

            // Map kind string to LSP SymbolKind enum
            int kindNum = 13; // Variable
            if (sym.kind == "function") kindNum = 12; // Function
            else if (sym.kind == "class") kindNum = 5; // Class
            else if (sym.kind == "interface") kindNum = 11; // Interface
            else if (sym.kind == "enum") kindNum = 10; // Enum
            else if (sym.kind == "method") kindNum = 6; // Method
            else if (sym.kind == "field") kindNum = 8; // Field
            symbol.set("kind", Json(kindNum));

            Json range = Json::object();
            Json start = Json::object();
            int defLine = std::max(0, static_cast<int>(sym.definitionLocation.line) - 1);
            int defCol = std::max(0, static_cast<int>(sym.definitionLocation.column) - 1);
            start.set("line", Json(defLine));
            start.set("character", Json(defCol));
            Json end = Json::object();
            end.set("line", Json(defLine));
            end.set("character", Json(defCol + static_cast<int>(sym.name.size())));
            range.set("start", start);
            range.set("end", end);
            symbol.set("range", range);
            symbol.set("selectionRange", range);

            if (!sym.detail.empty()) {
                symbol.set("detail", Json(sym.detail));
            }

            result.push(std::move(symbol));
        }
    }

    sendResponse(id, result);
}

// ============================================================================
// Analysis
// ============================================================================

void LspServer::analyzeDocument(const std::string& uri) {
    auto it = documents_.find(uri);
    if (it == documents_.end()) return;

    Document& doc = it->second;
    std::string filePath = uriToPath(uri);

    DiagnosticEngine diagnostics;

    // Phase 1: Lex
    Lexer lexer(doc.content, filePath, diagnostics);
    auto tokens = lexer.tokenize();

    if (diagnostics.hasErrors()) {
        publishDiagnostics(uri, diagnostics.diagnostics());
        return;
    }

    // Phase 2: Parse
    Parser parser(tokens, diagnostics);
    doc.program = parser.parse();
    doc.parsed = !diagnostics.hasErrors();

    if (diagnostics.hasErrors()) {
        publishDiagnostics(uri, diagnostics.diagnostics());
        return;
    }

    // Phase 3: Type check
    TypeChecker checker(diagnostics);
    checker.check(doc.program);

    // Collect symbols for navigation
    collectSymbols(doc.program, filePath);

    // Publish all diagnostics (errors + warnings)
    publishDiagnostics(uri, diagnostics.diagnostics());
}

void LspServer::publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diags) {
    Json diagnosticsArray = Json::array();

    for (auto& diag : diags) {
        Json d = Json::object();

        // Range
        Json range = Json::object();
        Json start = Json::object();
        int line = std::max(0, static_cast<int>(diag.location.line) - 1); // LSP is 0-based
        int col = std::max(0, static_cast<int>(diag.location.column) - 1);
        start.set("line", Json(line));
        start.set("character", Json(col));
        Json end = Json::object();
        end.set("line", Json(line));
        end.set("character", Json(col + 1));
        range.set("start", start);
        range.set("end", end);
        d.set("range", range);

        // Severity: 1=Error, 2=Warning, 3=Info, 4=Hint
        int severity = 1;
        switch (diag.severity) {
            case DiagnosticSeverity::Error: severity = 1; break;
            case DiagnosticSeverity::Warning: severity = 2; break;
            case DiagnosticSeverity::Info: severity = 3; break;
        }
        d.set("severity", Json(severity));
        d.set("source", Json("chrisplusplus"));
        d.set("code", Json(diag.code));
        d.set("message", Json(diag.message));

        diagnosticsArray.push(std::move(d));
    }

    Json params = Json::object();
    params.set("uri", Json(uri));
    params.set("diagnostics", diagnosticsArray);
    sendNotification("textDocument/publishDiagnostics", params);
}

void LspServer::collectSymbols(const Program& program, const std::string& file) {
    std::string uri = pathToUri(file);
    auto& syms = symbols_[uri];
    syms.clear();

    for (auto& decl : program.declarations) {
        if (auto* func = dynamic_cast<FuncDecl*>(decl.get())) {
            SymbolInfo sym;
            sym.name = func->name;
            sym.kind = "function";

            // Build signature
            std::string sig = "func " + func->name + "(";
            for (size_t i = 0; i < func->parameters.size(); i++) {
                if (i > 0) sig += ", ";
                sig += func->parameters[i].name;
                if (func->parameters[i].type) {
                    sig += ": " + func->parameters[i].type->toString();
                }
            }
            sig += ")";
            if (func->returnType) {
                sig += " -> " + func->returnType->toString();
            }
            sym.detail = sig;
            sym.definitionLocation = func->location;
            syms.push_back(std::move(sym));
        } else if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            SymbolInfo sym;
            sym.name = cls->name;
            sym.kind = "class";
            sym.definitionLocation = cls->location;

            std::string detail = "class " + cls->name;
            if (!cls->baseClass.empty()) detail += " : " + cls->baseClass;
            sym.detail = detail;
            syms.push_back(std::move(sym));

            // Add fields and methods as child symbols
            for (auto& field : cls->fields) {
                SymbolInfo fieldSym;
                fieldSym.name = cls->name + "." + field->name;
                fieldSym.kind = "field";
                fieldSym.definitionLocation = field->location;
                if (field->typeAnnotation) {
                    fieldSym.detail = field->name + ": " + field->typeAnnotation->toString();
                }
                syms.push_back(std::move(fieldSym));
            }
            for (auto& method : cls->methods) {
                SymbolInfo methodSym;
                methodSym.name = cls->name + "." + method->name;
                methodSym.kind = "method";
                methodSym.definitionLocation = method->location;

                std::string sig = "func " + method->name + "(";
                for (size_t i = 0; i < method->parameters.size(); i++) {
                    if (i > 0) sig += ", ";
                    sig += method->parameters[i].name;
                    if (method->parameters[i].type) {
                        sig += ": " + method->parameters[i].type->toString();
                    }
                }
                sig += ")";
                if (method->returnType) sig += " -> " + method->returnType->toString();
                methodSym.detail = sig;
                syms.push_back(std::move(methodSym));
            }
        } else if (auto* iface = dynamic_cast<InterfaceDecl*>(decl.get())) {
            SymbolInfo sym;
            sym.name = iface->name;
            sym.kind = "interface";
            sym.detail = "interface " + iface->name;
            sym.definitionLocation = iface->location;
            syms.push_back(std::move(sym));
        } else if (auto* enumDecl = dynamic_cast<EnumDecl*>(decl.get())) {
            SymbolInfo sym;
            sym.name = enumDecl->name;
            sym.kind = "enum";
            sym.detail = "enum " + enumDecl->name;
            sym.definitionLocation = enumDecl->location;
            syms.push_back(std::move(sym));

            // Add enum variants as child symbols
            for (auto& variant : enumDecl->variants) {
                SymbolInfo variantSym;
                variantSym.name = enumDecl->name + "." + variant.name;
                variantSym.kind = "enumMember";
                variantSym.detail = enumDecl->name + "." + variant.name;
                if (variant.location.line > 0) {
                    variantSym.definitionLocation = variant.location;
                } else {
                    variantSym.definitionLocation = enumDecl->location;
                }
                syms.push_back(std::move(variantSym));
            }
        } else if (auto* externFunc = dynamic_cast<ExternFuncDecl*>(decl.get())) {
            SymbolInfo sym;
            sym.name = externFunc->name;
            sym.kind = "function";

            std::string sig = "extern func " + externFunc->name + "(";
            for (size_t i = 0; i < externFunc->parameters.size(); i++) {
                if (i > 0) sig += ", ";
                sig += externFunc->parameters[i].name;
                if (externFunc->parameters[i].type) {
                    sig += ": " + externFunc->parameters[i].type->toString();
                }
            }
            sig += ")";
            if (externFunc->returnType) {
                sig += " -> " + externFunc->returnType->toString();
            }
            sym.detail = sig;
            sym.definitionLocation = externFunc->location;
            syms.push_back(std::move(sym));
        } else if (auto* var = dynamic_cast<VarDecl*>(decl.get())) {
            SymbolInfo sym;
            sym.name = var->name;
            sym.kind = "variable";
            sym.definitionLocation = var->location;
            if (var->typeAnnotation) {
                sym.detail = (var->isMutable ? "var " : "let ") + var->name + ": " + var->typeAnnotation->toString();
            }
            syms.push_back(std::move(sym));
        }
    }
}

SymbolInfo* LspServer::findSymbolAt(const std::string& uri, int line, int character) {
    auto it = symbols_.find(uri);
    if (it == symbols_.end()) return nullptr;

    // Find the symbol whose definition location matches the line
    // This is a simple heuristic — match by line number
    for (auto& sym : it->second) {
        if (static_cast<int>(sym.definitionLocation.line) == line) {
            return &sym;
        }
    }

    // Try to find a symbol by name in the document content at the given position
    auto docIt = documents_.find(uri);
    if (docIt == documents_.end()) return nullptr;

    const std::string& content = docIt->second.content;
    // Find the line
    int currentLine = 1;
    size_t lineStart = 0;
    for (size_t i = 0; i < content.size() && currentLine < line; i++) {
        if (content[i] == '\n') {
            currentLine++;
            lineStart = i + 1;
        }
    }

    // Extract word at position
    size_t pos = lineStart + character - 1;
    if (pos >= content.size()) return nullptr;

    size_t wordStart = pos;
    while (wordStart > lineStart && (std::isalnum(content[wordStart - 1]) || content[wordStart - 1] == '_')) {
        wordStart--;
    }
    size_t wordEnd = pos;
    while (wordEnd < content.size() && (std::isalnum(content[wordEnd]) || content[wordEnd] == '_')) {
        wordEnd++;
    }
    std::string word = content.substr(wordStart, wordEnd - wordStart);

    if (word.empty()) return nullptr;

    // Try to extract dotted expression (e.g. Color.Blue)
    // Look backward from wordStart for a dot and preceding identifier
    std::string dottedName;
    if (wordStart > 1 && content[wordStart - 1] == '.') {
        size_t objEnd = wordStart - 1;
        size_t objStart = objEnd;
        while (objStart > lineStart && (std::isalnum(content[objStart - 1]) || content[objStart - 1] == '_')) {
            objStart--;
        }
        if (objStart < objEnd) {
            std::string objName = content.substr(objStart, objEnd - objStart);
            dottedName = objName + "." + word;
        }
    }

    // Search for matching symbol — try dotted name first, then plain name
    if (!dottedName.empty()) {
        for (auto& sym : it->second) {
            if (sym.name == dottedName) {
                return &sym;
            }
        }
    }

    for (auto& sym : it->second) {
        if (sym.name == word || sym.name.substr(sym.name.rfind('.') + 1) == word) {
            return &sym;
        }
    }

    return nullptr;
}

std::vector<Json> LspServer::getCompletionItems(const std::string& uri, int line, int character) {
    std::vector<Json> items;

    // Keywords
    static const std::vector<std::pair<std::string, std::string>> keywords = {
        {"func", "Function declaration"},
        {"var", "Mutable variable"},
        {"let", "Immutable variable"},
        {"class", "Class declaration"},
        {"interface", "Interface declaration"},
        {"enum", "Enum declaration"},
        {"if", "Conditional statement"},
        {"else", "Else branch"},
        {"for", "For loop"},
        {"while", "While loop"},
        {"return", "Return statement"},
        {"import", "Import declaration"},
        {"public", "Public access modifier"},
        {"private", "Private access modifier"},
        {"protected", "Protected access modifier"},
        {"throw", "Throw exception"},
        {"try", "Try block"},
        {"catch", "Catch block"},
        {"finally", "Finally block"},
        {"async", "Async function"},
        {"await", "Await expression"},
        {"io", "IO-bound async"},
        {"compute", "Compute-bound async"},
        {"unsafe", "Unsafe block"},
        {"shared", "Thread-safe class"},
        {"match", "Match expression"},
        {"operator", "Operator overload"},
        {"extern", "External function"},
        {"break", "Break loop"},
        {"continue", "Continue loop"},
        {"nil", "Nil literal"},
        {"true", "Boolean true"},
        {"false", "Boolean false"},
        {"this", "Current instance"},
    };

    for (auto& [kw, doc] : keywords) {
        Json item = Json::object();
        item.set("label", Json(kw));
        item.set("kind", Json(14)); // Keyword
        item.set("detail", Json(doc));
        items.push_back(std::move(item));
    }

    // Built-in types
    static const std::vector<std::string> types = {
        "Int", "Int8", "Int16", "Int32",
        "UInt", "UInt8", "UInt16", "UInt32",
        "Float", "Float32", "Bool", "String", "Char", "Void",
        "Array", "Map", "Set"
    };

    for (auto& t : types) {
        Json item = Json::object();
        item.set("label", Json(t));
        item.set("kind", Json(7)); // Class (type)
        item.set("detail", Json("Built-in type"));
        items.push_back(std::move(item));
    }

    // Built-in functions
    static const std::vector<std::pair<std::string, std::string>> builtins = {
        {"print", "func print(value: String)"},
        {"typeof", "func typeof(obj) -> TypeInfo"},
        {"readLine", "func readLine() -> String"},
        {"readFile", "func readFile(path: String) -> String"},
        {"writeFile", "func writeFile(path: String, content: String) -> Bool"},
        {"appendFile", "func appendFile(path: String, content: String) -> Bool"},
        {"fileExists", "func fileExists(path: String) -> Bool"},
        {"exec", "func exec(cmd: String) -> Int"},
        {"execOutput", "func execOutput(cmd: String) -> String"},
        {"abs", "func abs(x: Int) -> Int"},
        {"min", "func min(a: Int, b: Int) -> Int"},
        {"max", "func max(a: Int, b: Int) -> Int"},
        {"random", "func random() -> Int"},
        {"sqrt", "func sqrt(x: Float) -> Float"},
        {"pow", "func pow(base: Float, exp: Float) -> Float"},
        {"floor", "func floor(x: Float) -> Float"},
        {"ceil", "func ceil(x: Float) -> Float"},
        {"round", "func round(x: Float) -> Float"},
        {"log", "func log(x: Float) -> Float"},
        {"sin", "func sin(x: Float) -> Float"},
        {"cos", "func cos(x: Float) -> Float"},
        {"tan", "func tan(x: Float) -> Float"},
        {"assert", "func assert(cond: Bool, msg: String)"},
        {"assertEqual", "func assertEqual(a, b, msg: String)"},
        {"assertTrue", "func assertTrue(cond: Bool, msg: String)"},
        {"assertFalse", "func assertFalse(cond: Bool, msg: String)"},
        {"jsonParse", "func jsonParse(str: String) -> Int"},
        {"jsonGet", "func jsonGet(handle: Int, key: String) -> String"},
        {"jsonStringify", "func jsonStringify(handle: Int) -> String"},
    };

    for (auto& [name, sig] : builtins) {
        Json item = Json::object();
        item.set("label", Json(name));
        item.set("kind", Json(3)); // Function
        item.set("detail", Json(sig));
        items.push_back(std::move(item));
    }

    // Symbols from the current document
    auto symIt = symbols_.find(uri);
    if (symIt != symbols_.end()) {
        for (auto& sym : symIt->second) {
            Json item = Json::object();
            item.set("label", Json(sym.name));

            int kindNum = 6; // Variable
            if (sym.kind == "function") kindNum = 3; // Function
            else if (sym.kind == "class") kindNum = 7; // Class
            else if (sym.kind == "interface") kindNum = 8; // Interface
            else if (sym.kind == "enum") kindNum = 13; // Enum
            else if (sym.kind == "method") kindNum = 2; // Method
            else if (sym.kind == "field") kindNum = 5; // Field
            item.set("kind", Json(kindNum));

            if (!sym.detail.empty()) {
                item.set("detail", Json(sym.detail));
            }

            items.push_back(std::move(item));
        }
    }

    return items;
}

std::string LspServer::uriToPath(const std::string& uri) const {
    // Convert file:///path/to/file -> /path/to/file
    if (uri.substr(0, 7) == "file://") {
        std::string path = uri.substr(7);
        // Decode percent-encoded characters
        std::string decoded;
        for (size_t i = 0; i < path.size(); i++) {
            if (path[i] == '%' && i + 2 < path.size()) {
                int hex = std::stoi(path.substr(i + 1, 2), nullptr, 16);
                decoded += static_cast<char>(hex);
                i += 2;
            } else {
                decoded += path[i];
            }
        }
        return decoded;
    }
    return uri;
}

std::string LspServer::pathToUri(const std::string& path) const {
    return "file://" + path;
}

} // namespace lsp
} // namespace chris
