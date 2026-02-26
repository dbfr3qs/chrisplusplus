#include "parser/parser.h"
#include <sstream>
#include <stdexcept>

namespace chris {

Parser::Parser(const std::vector<Token>& tokens, DiagnosticEngine& diagnostics)
    : tokens_(tokens), diagnostics_(diagnostics), pos_(0) {}

// --- Token navigation ---

const Token& Parser::current() const {
    if (pos_ >= tokens_.size()) return tokens_.back();
    return tokens_[pos_];
}

const Token& Parser::peek() const {
    size_t next = pos_ + 1;
    if (next >= tokens_.size()) return tokens_.back();
    return tokens_[next];
}

const Token& Parser::advance() {
    const Token& tok = current();
    if (!isAtEnd()) pos_++;
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    diagnostics_.error("E2001", message, current().location);
    return Token(TokenType::Error, "", current().location);
}

Token Parser::expectIdentifierOrKeyword(const std::string& message) {
    // Accept identifiers and keywords as valid names (e.g. for import paths)
    if (check(TokenType::Identifier) || current().isKeyword()) {
        return advance();
    }
    diagnostics_.error("E2001", message, current().location);
    return Token(TokenType::Error, "", current().location);
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::EndOfFile;
}

void Parser::skipComments() {
    while (check(TokenType::LineComment) ||
           check(TokenType::BlockComment) ||
           check(TokenType::DocComment)) {
        advance();
    }
}

// --- Top-level ---

Program Parser::parse() {
    Program program;
    skipComments();
    while (!isAtEnd()) {
        auto decl = parseDeclaration();
        if (decl) {
            program.declarations.push_back(std::move(decl));
        }
        skipComments();
    }
    return program;
}

// --- Annotations ---

std::vector<Annotation> Parser::parseAnnotations() {
    std::vector<Annotation> annotations;
    while (check(TokenType::At)) {
        Annotation ann;
        ann.location = current().location;
        advance(); // consume '@'
        Token name = expect(TokenType::Identifier, "Expected annotation name after '@'");
        ann.name = name.value;
        // Optional arguments: @Name("arg1", "arg2")
        if (match(TokenType::LeftParen)) {
            if (!check(TokenType::RightParen)) {
                do {
                    if (check(TokenType::StringLiteral)) {
                        ann.arguments.push_back(current().value);
                        advance();
                    } else if (check(TokenType::Identifier)) {
                        ann.arguments.push_back(current().value);
                        advance();
                    } else if (check(TokenType::IntLiteral)) {
                        ann.arguments.push_back(current().value);
                        advance();
                    } else {
                        diagnostics_.error("E2001", "Expected annotation argument", current().location);
                        break;
                    }
                } while (match(TokenType::Comma));
            }
            expect(TokenType::RightParen, "Expected ')' after annotation arguments");
        }
        annotations.push_back(std::move(ann));
        skipComments();
    }
    return annotations;
}

StmtPtr Parser::parseDeclaration() {
    skipComments();
    if (isAtEnd()) return nullptr;

    // Parse any leading annotations
    auto annotations = parseAnnotations();
    skipComments();

    if (check(TokenType::KwImport)) {
        return parseImportDecl();
    }
    if (check(TokenType::KwPublic) || check(TokenType::KwPrivate)) {
        bool isPublic = check(TokenType::KwPublic);
        advance(); // consume access modifier
        skipComments();
        if (check(TokenType::KwShared)) {
            advance(); // consume 'shared'
            skipComments();
            auto cls = parseClassDecl(isPublic);
            if (cls) {
                auto* classDecl = dynamic_cast<ClassDecl*>(cls.get());
                if (classDecl) {
                    classDecl->isShared = true;
                    classDecl->annotations = std::move(annotations);
                }
            }
            return cls;
        }
        if (check(TokenType::KwClass)) {
            auto cls = parseClassDecl(isPublic);
            if (cls) cls->annotations = std::move(annotations);
            return cls;
        }
        if (check(TokenType::KwAsync)) {
            advance(); // consume 'async'
            skipComments();
            auto func = parseFuncDecl();
            if (func) {
                func->isAsync = true;
                func->annotations = std::move(annotations);
            }
            return func;
        }
        if (check(TokenType::KwFunc)) {
            auto func = parseFuncDecl();
            if (func) func->annotations = std::move(annotations);
            return func;
        }
        diagnostics_.error("E2001", "Expected 'class' or 'func' after access modifier", current().location);
        return nullptr;
    }
    if (check(TokenType::KwInterface)) {
        auto iface = parseInterfaceDecl();
        if (iface) {
            auto* ifaceDecl = dynamic_cast<InterfaceDecl*>(iface.get());
            if (ifaceDecl) ifaceDecl->annotations = std::move(annotations);
        }
        return iface;
    }
    if (check(TokenType::KwEnum)) {
        auto en = parseEnumDecl();
        if (en) {
            auto* enumDecl = dynamic_cast<EnumDecl*>(en.get());
            if (enumDecl) enumDecl->annotations = std::move(annotations);
        }
        return en;
    }
    if (check(TokenType::KwExtern)) {
        return parseExternFuncDecl();
    }
    if (check(TokenType::KwShared)) {
        advance(); // consume 'shared'
        skipComments();
        auto cls = parseClassDecl(false);
        if (cls) {
            auto* classDecl = dynamic_cast<ClassDecl*>(cls.get());
            if (classDecl) {
                classDecl->isShared = true;
                classDecl->annotations = std::move(annotations);
            }
        }
        return cls;
    }
    if (check(TokenType::KwClass)) {
        auto cls = parseClassDecl(false);
        if (cls) cls->annotations = std::move(annotations);
        return cls;
    }
    if (check(TokenType::KwAsync)) {
        advance(); // consume 'async'
        skipComments();
        auto func = parseFuncDecl();
        if (func) {
            func->isAsync = true;
            func->annotations = std::move(annotations);
        }
        return func;
    }
    if (check(TokenType::KwFunc)) {
        auto func = parseFuncDecl();
        if (func) func->annotations = std::move(annotations);
        return func;
    }

    // At top level, also allow statements for scripting style
    return parseStatement();
}

StmtPtr Parser::parseImportDecl() {
    SourceLocation loc = current().location;
    advance(); // consume 'import'

    std::string path;
    if (check(TokenType::StringLiteral)) {
        // File import: import "path/to/file.chr";
        path = current().value;
        advance();
    } else {
        // Module path segments can be identifiers or keywords (e.g. "std.io")
        Token name = expectIdentifierOrKeyword("Expected module name or string path after 'import'");
        path = name.value;

        while (match(TokenType::Dot)) {
            Token part = expectIdentifierOrKeyword("Expected identifier after '.'");
            path += "." + part.value;
        }
    }

    expect(TokenType::Semicolon, "Expected ';' after import statement");

    auto decl = std::make_unique<ImportDecl>();
    decl->location = loc;
    decl->path = path;
    return decl;
}

std::unique_ptr<InterfaceDecl> Parser::parseInterfaceDecl() {
    SourceLocation loc = current().location;
    advance(); // consume 'interface'

    Token name = expect(TokenType::Identifier, "Expected interface name after 'interface'");

    auto ifaceDecl = std::make_unique<InterfaceDecl>();
    ifaceDecl->location = loc;
    ifaceDecl->name = name.value;

    expect(TokenType::LeftBrace, "Expected '{' after interface name");
    skipComments();

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        skipComments();

        // Optional access modifier
        if (check(TokenType::KwPublic) || check(TokenType::KwPrivate) || check(TokenType::KwProtected)) {
            advance();
            skipComments();
        }

        if (check(TokenType::KwFunc)) {
            auto method = parseFuncDecl();
            if (method) {
                ifaceDecl->methods.push_back(std::move(method));
            }
        } else {
            diagnostics_.error("E2001", "Expected method declaration in interface body", current().location);
            advance();
        }
        skipComments();
    }

    expect(TokenType::RightBrace, "Expected '}' after interface body");
    return ifaceDecl;
}

std::unique_ptr<EnumDecl> Parser::parseEnumDecl() {
    SourceLocation loc = current().location;
    advance(); // consume 'enum'

    Token name = expect(TokenType::Identifier, "Expected enum name after 'enum'");

    auto enumDecl = std::make_unique<EnumDecl>();
    enumDecl->location = loc;
    enumDecl->name = name.value;

    expect(TokenType::LeftBrace, "Expected '{' after enum name");
    skipComments();

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        skipComments();
        // Optional 'case' keyword before each variant
        match(TokenType::KwCase);
        Token caseName = expect(TokenType::Identifier, "Expected enum case name");
        enumDecl->cases.push_back(caseName.value);

        // Check for associated value type: Ok(Int)
        EnumVariant variant;
        variant.name = caseName.value;
        variant.location = caseName.location;
        if (match(TokenType::LeftParen)) {
            variant.associatedType = parseTypeExpr();
            expect(TokenType::RightParen, "Expected ')' after associated type");
        }
        enumDecl->variants.push_back(std::move(variant));

        // Optional trailing comma
        if (check(TokenType::Comma)) {
            advance();
        }
        skipComments();
    }

    expect(TokenType::RightBrace, "Expected '}' after enum body");
    return enumDecl;
}

std::unique_ptr<ClassDecl> Parser::parseClassDecl(bool isPublic) {
    SourceLocation loc = current().location;
    advance(); // consume 'class'

    Token name = expect(TokenType::Identifier, "Expected class name after 'class'");

    auto classDecl = std::make_unique<ClassDecl>();
    classDecl->location = loc;
    classDecl->name = name.value;
    classDecl->isPublic = isPublic;

    // Parse optional generic type parameters: class Box<T> or class Pair<K, V>
    if (check(TokenType::Less)) {
        advance(); // consume '<'
        do {
            Token param = expect(TokenType::Identifier, "Expected type parameter name");
            classDecl->typeParams.push_back(param.value);
        } while (match(TokenType::Comma));
        expect(TokenType::Greater, "Expected '>' after type parameters");
    }

    // Parse inheritance: class Dog : Animal or class Dog : Animal, Printable
    if (match(TokenType::Colon)) {
        // First name could be a base class or interface
        Token baseName = expect(TokenType::Identifier, "Expected base class or interface name after ':'");
        classDecl->baseClass = baseName.value;

        // Additional interfaces after comma
        while (match(TokenType::Comma)) {
            Token ifaceName = expect(TokenType::Identifier, "Expected interface name after ','");
            classDecl->interfaces.push_back(ifaceName.value);
        }
    }

    expect(TokenType::LeftBrace, "Expected '{' after class name");
    skipComments();

    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        skipComments();

        // Parse optional access modifier for members
        AccessModifier memberAccess = AccessModifier::Private; // default
        if (check(TokenType::KwPublic)) {
            memberAccess = AccessModifier::Public;
            advance();
            skipComments();
        } else if (check(TokenType::KwProtected)) {
            memberAccess = AccessModifier::Protected;
            advance();
            skipComments();
        } else if (check(TokenType::KwPrivate)) {
            memberAccess = AccessModifier::Private;
            advance();
            skipComments();
        }

        if (check(TokenType::KwVar) || check(TokenType::KwLet)) {
            auto field = parseVarDecl();
            if (field) {
                field->access = memberAccess;
                classDecl->fields.push_back(std::move(field));
            }
        } else if (check(TokenType::KwAsync)) {
            advance(); // consume 'async'
            skipComments();
            auto method = parseFuncDecl();
            if (method) {
                method->isAsync = true;
                method->access = memberAccess;
                classDecl->methods.push_back(std::move(method));
            }
        } else if (check(TokenType::KwFunc)) {
            auto method = parseFuncDecl();
            if (method) {
                method->access = memberAccess;
                classDecl->methods.push_back(std::move(method));
            }
        } else if (check(TokenType::KwOperator)) {
            auto opMethod = parseOperatorDecl();
            if (opMethod) {
                opMethod->access = memberAccess;
                classDecl->methods.push_back(std::move(opMethod));
            }
        } else {
            diagnostics_.error("E2001", "Expected field or method declaration in class body", current().location);
            advance();
        }
        skipComments();
    }

    expect(TokenType::RightBrace, "Expected '}' after class body");
    return classDecl;
}

std::unique_ptr<FuncDecl> Parser::parseOperatorDecl() {
    SourceLocation loc = current().location;
    advance(); // consume 'operator'

    // Parse the operator symbol: +, -, *, /, %, ==, !=, <, >, <=, >=, []
    std::string opSymbol;
    switch (current().type) {
        case TokenType::Plus:       opSymbol = "+"; advance(); break;
        case TokenType::Minus:      opSymbol = "-"; advance(); break;
        case TokenType::Star:       opSymbol = "*"; advance(); break;
        case TokenType::Slash:      opSymbol = "/"; advance(); break;
        case TokenType::Percent:    opSymbol = "%"; advance(); break;
        case TokenType::Equal:      opSymbol = "=="; advance(); break;
        case TokenType::NotEqual:   opSymbol = "!="; advance(); break;
        case TokenType::Less:       opSymbol = "<"; advance(); break;
        case TokenType::Greater:    opSymbol = ">"; advance(); break;
        case TokenType::LessEqual:  opSymbol = "<="; advance(); break;
        case TokenType::GreaterEqual: opSymbol = ">="; advance(); break;
        case TokenType::LeftBracket:
            opSymbol = "[]";
            advance();
            expect(TokenType::RightBracket, "Expected ']' after '['");
            break;
        default:
            diagnostics_.error("E2001", "Expected operator symbol after 'operator'", current().location);
            advance();
            return nullptr;
    }

    expect(TokenType::LeftParen, "Expected '(' after operator symbol");

    std::vector<Parameter> params;
    if (!check(TokenType::RightParen)) {
        do {
            skipComments();
            Parameter param;
            param.location = current().location;
            Token paramName = expect(TokenType::Identifier, "Expected parameter name");
            param.name = paramName.value;
            expect(TokenType::Colon, "Expected ':' after parameter name");
            param.type = parseTypeExpr();
            params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RightParen, "Expected ')' after parameters");

    TypeExprPtr returnType = nullptr;
    if (match(TokenType::Arrow)) {
        returnType = parseTypeExpr();
    }

    auto body = parseBlock();

    auto func = std::make_unique<FuncDecl>();
    func->location = loc;
    func->name = "operator" + opSymbol;
    func->isOperator = true;
    func->parameters = std::move(params);
    func->returnType = std::move(returnType);
    func->body = std::move(body);
    return func;
}

std::unique_ptr<ExternFuncDecl> Parser::parseExternFuncDecl() {
    SourceLocation loc = current().location;
    advance(); // consume 'extern'

    expect(TokenType::KwFunc, "Expected 'func' after 'extern'");
    Token name = expect(TokenType::Identifier, "Expected function name after 'extern func'");
    expect(TokenType::LeftParen, "Expected '(' after function name");

    std::vector<Parameter> params;
    bool isVariadic = false;
    if (!check(TokenType::RightParen)) {
        do {
            skipComments();
            // Check for variadic '...'
            if (check(TokenType::DotDot)) {
                // Actually we need '...' — let's check for Dot tokens
                advance(); // consume '..'
                // consume the third dot if present (DotDot + Dot or just treat DotDot as variadic marker)
                if (check(TokenType::Dot)) advance();
                isVariadic = true;
                break;
            }
            Parameter param;
            param.location = current().location;
            Token paramName = expect(TokenType::Identifier, "Expected parameter name");
            param.name = paramName.value;
            expect(TokenType::Colon, "Expected ':' after parameter name");
            param.type = parseTypeExpr();
            params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RightParen, "Expected ')' after parameters");

    TypeExprPtr returnType = nullptr;
    if (match(TokenType::Arrow)) {
        returnType = parseTypeExpr();
    }

    expect(TokenType::Semicolon, "Expected ';' after extern function declaration");

    auto decl = std::make_unique<ExternFuncDecl>();
    decl->location = loc;
    decl->name = name.value;
    decl->parameters = std::move(params);
    decl->returnType = std::move(returnType);
    decl->isVariadic = isVariadic;
    return decl;
}

std::unique_ptr<FuncDecl> Parser::parseFuncDecl() {
    SourceLocation loc = current().location;
    advance(); // consume 'func'

    // Allow 'new' keyword as a function name (for constructors)
    Token name = (check(TokenType::KwNew))
        ? advance()
        : expect(TokenType::Identifier, "Expected function name after 'func'");
    expect(TokenType::LeftParen, "Expected '(' after function name");

    std::vector<Parameter> params;
    if (!check(TokenType::RightParen)) {
        do {
            skipComments();
            Parameter param;
            param.location = current().location;
            Token paramName = expect(TokenType::Identifier, "Expected parameter name");
            param.name = paramName.value;
            expect(TokenType::Colon, "Expected ':' after parameter name");
            param.type = parseTypeExpr();
            params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RightParen, "Expected ')' after parameters");

    TypeExprPtr returnType = nullptr;
    AsyncKind asyncKind = AsyncKind::None;
    if (match(TokenType::Arrow)) {
        // Check for io/compute annotation before the return type
        if (check(TokenType::KwIo)) {
            asyncKind = AsyncKind::Io;
            advance();
            skipComments();
        } else if (check(TokenType::KwCompute)) {
            asyncKind = AsyncKind::Compute;
            advance();
            skipComments();
        }
        returnType = parseTypeExpr();
    }

    // Interface method signatures end with ';' instead of a body
    std::unique_ptr<Block> body;
    if (check(TokenType::LeftBrace)) {
        body = parseBlock();
    } else if (match(TokenType::Semicolon)) {
        // Bodyless signature (interface method)
        body = nullptr;
    } else {
        body = parseBlock(); // will produce error
    }

    auto func = std::make_unique<FuncDecl>();
    func->location = loc;
    func->name = name.value;
    func->asyncKind = asyncKind;
    func->parameters = std::move(params);
    func->returnType = std::move(returnType);
    func->body = std::move(body);
    return func;
}

// --- Statements ---

StmtPtr Parser::parseStatement() {
    skipComments();
    if (isAtEnd()) return nullptr;

    if (check(TokenType::KwVar) || check(TokenType::KwLet)) {
        return parseVarDecl();
    }
    if (check(TokenType::KwIf)) {
        return parseIfStmt();
    }
    if (check(TokenType::KwWhile)) {
        return parseWhileStmt();
    }
    if (check(TokenType::KwFor)) {
        return parseForStmt();
    }
    if (check(TokenType::KwReturn)) {
        return parseReturnStmt();
    }
    if (check(TokenType::KwBreak)) {
        return parseBreakStmt();
    }
    if (check(TokenType::KwContinue)) {
        return parseContinueStmt();
    }
    if (check(TokenType::LeftBrace)) {
        return parseBlock();
    }

    // Throw statement
    if (check(TokenType::KwThrow)) {
        return parseThrowStmt();
    }
    // Try/catch/finally
    if (check(TokenType::KwTry)) {
        return parseTryCatchStmt();
    }

    // Unsafe block
    if (check(TokenType::KwUnsafe)) {
        auto loc = current().location;
        advance(); // consume 'unsafe'
        skipComments();
        auto body = parseBlock();
        auto unsafeBlock = std::make_unique<UnsafeBlock>();
        unsafeBlock->location = loc;
        unsafeBlock->body.reset(static_cast<Block*>(body.release()));
        return unsafeBlock;
    }

    // Match expression used as a statement (no trailing semicolon needed)
    if (check(TokenType::KwMatch)) {
        auto matchExpr = parseExpression(); // parsePrimary handles match
        auto exprStmt = std::make_unique<ExprStmt>();
        exprStmt->location = matchExpr->location;
        exprStmt->expression = std::move(matchExpr);
        return exprStmt;
    }

    return parseExprStmt();
}

std::unique_ptr<VarDecl> Parser::parseVarDecl() {
    SourceLocation loc = current().location;
    bool isMutable = current().type == TokenType::KwVar;
    advance(); // consume var/let

    Token name = expect(TokenType::Identifier, "Expected variable name");

    TypeExprPtr typeAnnotation = nullptr;
    if (match(TokenType::Colon)) {
        typeAnnotation = parseTypeExpr();
    }

    ExprPtr initializer = nullptr;
    if (match(TokenType::Assign)) {
        initializer = parseExpression();
    }

    expect(TokenType::Semicolon, "Expected ';' after variable declaration");

    auto decl = std::make_unique<VarDecl>();
    decl->location = loc;
    decl->name = name.value;
    decl->isMutable = isMutable;
    decl->typeAnnotation = std::move(typeAnnotation);
    decl->initializer = std::move(initializer);
    return decl;
}

std::unique_ptr<IfStmt> Parser::parseIfStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'if'

    auto condition = parseExpression();
    auto thenBlock = parseBlock();

    StmtPtr elseBlock = nullptr;
    skipComments();
    if (match(TokenType::KwElse)) {
        skipComments();
        if (check(TokenType::KwIf)) {
            elseBlock = parseIfStmt();
        } else {
            elseBlock = parseBlock();
        }
    }

    auto stmt = std::make_unique<IfStmt>();
    stmt->location = loc;
    stmt->condition = std::move(condition);
    stmt->thenBlock = std::move(thenBlock);
    stmt->elseBlock = std::move(elseBlock);
    return stmt;
}

std::unique_ptr<WhileStmt> Parser::parseWhileStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'while'

    auto condition = parseExpression();
    auto body = parseBlock();

    auto stmt = std::make_unique<WhileStmt>();
    stmt->location = loc;
    stmt->condition = std::move(condition);
    stmt->body = std::move(body);
    return stmt;
}

std::unique_ptr<ForStmt> Parser::parseForStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'for'

    Token varName = expect(TokenType::Identifier, "Expected variable name after 'for'");
    expect(TokenType::KwIn, "Expected 'in' after for variable");

    auto iterable = parseExpression();
    auto body = parseBlock();

    auto stmt = std::make_unique<ForStmt>();
    stmt->location = loc;
    stmt->variable = varName.value;
    stmt->iterable = std::move(iterable);
    stmt->body = std::move(body);
    return stmt;
}

std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'return'

    ExprPtr value = nullptr;
    if (!check(TokenType::Semicolon) && !check(TokenType::RightBrace) && !isAtEnd()) {
        value = parseExpression();
    }

    expect(TokenType::Semicolon, "Expected ';' after return statement");

    auto stmt = std::make_unique<ReturnStmt>();
    stmt->location = loc;
    stmt->value = std::move(value);
    return stmt;
}

std::unique_ptr<BreakStmt> Parser::parseBreakStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'break'
    expect(TokenType::Semicolon, "Expected ';' after 'break'");

    auto stmt = std::make_unique<BreakStmt>();
    stmt->location = loc;
    return stmt;
}

std::unique_ptr<ContinueStmt> Parser::parseContinueStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'continue'
    expect(TokenType::Semicolon, "Expected ';' after 'continue'");

    auto stmt = std::make_unique<ContinueStmt>();
    stmt->location = loc;
    return stmt;
}

std::unique_ptr<ThrowStmt> Parser::parseThrowStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'throw'

    auto expr = parseExpression();
    expect(TokenType::Semicolon, "Expected ';' after throw expression");

    auto stmt = std::make_unique<ThrowStmt>();
    stmt->location = loc;
    stmt->expression = std::move(expr);
    return stmt;
}

std::unique_ptr<TryCatchStmt> Parser::parseTryCatchStmt() {
    SourceLocation loc = current().location;
    advance(); // consume 'try'

    auto stmt = std::make_unique<TryCatchStmt>();
    stmt->location = loc;
    stmt->tryBlock = parseBlock();

    // Parse catch clauses
    while (check(TokenType::KwCatch)) {
        advance(); // consume 'catch'
        expect(TokenType::LeftParen, "Expected '(' after 'catch'");

        CatchClause clause;
        Token varName = expect(TokenType::Identifier, "Expected variable name in catch");
        clause.varName = varName.value;
        expect(TokenType::Colon, "Expected ':' after catch variable name");
        Token typeName = expect(TokenType::Identifier, "Expected type name in catch");
        clause.typeName = typeName.value;

        expect(TokenType::RightParen, "Expected ')' after catch declaration");
        clause.body = parseBlock();

        stmt->catchClauses.push_back(std::move(clause));
    }

    // Parse optional finally block
    if (check(TokenType::KwFinally)) {
        advance(); // consume 'finally'
        stmt->finallyBlock = parseBlock();
    }

    return stmt;
}

std::unique_ptr<Block> Parser::parseBlock() {
    skipComments();
    expect(TokenType::LeftBrace, "Expected '{'");

    auto block = std::make_unique<Block>();
    block->location = current().location;

    skipComments();
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
        skipComments();
    }

    expect(TokenType::RightBrace, "Expected '}'");
    return block;
}

std::unique_ptr<ExprStmt> Parser::parseExprStmt() {
    auto expr = parseExpression();
    expect(TokenType::Semicolon, "Expected ';' after expression");

    auto stmt = std::make_unique<ExprStmt>();
    stmt->location = expr->location;
    stmt->expression = std::move(expr);
    return stmt;
}

// --- Expressions ---

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    auto expr = parseRange();

    if (check(TokenType::Assign)) {
        SourceLocation loc = current().location;
        advance();
        auto value = parseAssignment(); // right-associative

        auto assign = std::make_unique<AssignExpr>();
        assign->location = loc;
        assign->target = std::move(expr);
        assign->value = std::move(value);
        return assign;
    }

    // Compound assignment: +=, -=, *=, /=, %=
    // Desugar: x += expr  =>  x = x + expr
    if (check(TokenType::PlusAssign) || check(TokenType::MinusAssign) ||
        check(TokenType::StarAssign) || check(TokenType::SlashAssign) ||
        check(TokenType::PercentAssign)) {
        SourceLocation loc = current().location;
        std::string op;
        switch (current().type) {
            case TokenType::PlusAssign:    op = "+"; break;
            case TokenType::MinusAssign:   op = "-"; break;
            case TokenType::StarAssign:    op = "*"; break;
            case TokenType::SlashAssign:   op = "/"; break;
            case TokenType::PercentAssign: op = "%"; break;
            default: break;
        }
        advance();
        auto rhs = parseAssignment();

        // Clone the target expression for use in the binary op
        // We need a copy of expr for the left side of the binary op
        // Since expr is already consumed as target, we create an identifier clone
        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;

        // Deep-clone the target for the binary LHS
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.get())) {
            auto clone = std::make_unique<IdentifierExpr>();
            clone->location = ident->location;
            clone->name = ident->name;
            binary->left = std::move(clone);
        } else if (auto* idx = dynamic_cast<IndexExpr*>(expr.get())) {
            auto clone = std::make_unique<IndexExpr>();
            clone->location = idx->location;
            // Shallow clone is sufficient since we only read
            auto objClone = std::make_unique<IdentifierExpr>();
            if (auto* objIdent = dynamic_cast<IdentifierExpr*>(idx->object.get())) {
                objClone->name = objIdent->name;
                objClone->location = objIdent->location;
            }
            clone->object = std::move(objClone);
            // Clone index
            if (auto* idxIdent = dynamic_cast<IdentifierExpr*>(idx->index.get())) {
                auto idxClone = std::make_unique<IdentifierExpr>();
                idxClone->name = idxIdent->name;
                idxClone->location = idxIdent->location;
                clone->index = std::move(idxClone);
            } else if (auto* idxLit = dynamic_cast<IntLiteralExpr*>(idx->index.get())) {
                auto litClone = std::make_unique<IntLiteralExpr>();
                litClone->value = idxLit->value;
                litClone->location = idxLit->location;
                clone->index = std::move(litClone);
            }
            binary->left = std::move(clone);
        }

        binary->right = std::move(rhs);

        auto assign = std::make_unique<AssignExpr>();
        assign->location = loc;
        assign->target = std::move(expr);
        assign->value = std::move(binary);
        return assign;
    }

    return expr;
}

ExprPtr Parser::parseRange() {
    auto expr = parseNilCoalesce();

    if (check(TokenType::DotDot)) {
        SourceLocation loc = current().location;
        advance();
        auto end = parseOr();

        auto range = std::make_unique<RangeExpr>();
        range->location = loc;
        range->start = std::move(expr);
        range->end = std::move(end);
        return range;
    }

    return expr;
}

ExprPtr Parser::parseNilCoalesce() {
    auto expr = parseOr();

    while (check(TokenType::DoubleQuestion)) {
        SourceLocation loc = current().location;
        advance();
        auto defaultVal = parseOr();

        auto coalesce = std::make_unique<NilCoalesceExpr>();
        coalesce->location = loc;
        coalesce->value = std::move(expr);
        coalesce->defaultValue = std::move(defaultVal);
        expr = std::move(coalesce);
    }

    return expr;
}

ExprPtr Parser::parseOr() {
    auto left = parseAnd();

    while (check(TokenType::Or)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto right = parseAnd();

        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;
        binary->left = std::move(left);
        binary->right = std::move(right);
        left = std::move(binary);
    }

    return left;
}

ExprPtr Parser::parseAnd() {
    auto left = parseEquality();

    while (check(TokenType::And)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto right = parseEquality();

        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;
        binary->left = std::move(left);
        binary->right = std::move(right);
        left = std::move(binary);
    }

    return left;
}

ExprPtr Parser::parseEquality() {
    auto left = parseComparison();

    while (check(TokenType::Equal) || check(TokenType::NotEqual)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto right = parseComparison();

        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;
        binary->left = std::move(left);
        binary->right = std::move(right);
        left = std::move(binary);
    }

    return left;
}

ExprPtr Parser::parseComparison() {
    auto left = parseAddition();

    while (check(TokenType::Less) || check(TokenType::Greater) ||
           check(TokenType::LessEqual) || check(TokenType::GreaterEqual)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto right = parseAddition();

        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;
        binary->left = std::move(left);
        binary->right = std::move(right);
        left = std::move(binary);
    }

    return left;
}

ExprPtr Parser::parseAddition() {
    auto left = parseMultiplication();

    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto right = parseMultiplication();

        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;
        binary->left = std::move(left);
        binary->right = std::move(right);
        left = std::move(binary);
    }

    return left;
}

ExprPtr Parser::parseMultiplication() {
    auto left = parseUnary();

    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Percent)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto right = parseUnary();

        auto binary = std::make_unique<BinaryExpr>();
        binary->location = loc;
        binary->op = op;
        binary->left = std::move(left);
        binary->right = std::move(right);
        left = std::move(binary);
    }

    return left;
}

ExprPtr Parser::parseUnary() {
    // await expression: await expr
    if (check(TokenType::KwAwait)) {
        SourceLocation loc = current().location;
        advance(); // consume 'await'
        auto operand = parseUnary();

        auto awaitExpr = std::make_unique<AwaitExpr>();
        awaitExpr->location = loc;
        awaitExpr->operand = std::move(operand);
        return awaitExpr;
    }

    if (check(TokenType::Minus) || check(TokenType::Not)) {
        SourceLocation loc = current().location;
        std::string op = current().value;
        advance();
        auto operand = parseUnary();

        auto unary = std::make_unique<UnaryExpr>();
        unary->location = loc;
        unary->op = op;
        unary->operand = std::move(operand);
        return unary;
    }

    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        if (check(TokenType::LeftParen)) {
            // Function call
            advance();
            auto args = parseArguments();
            expect(TokenType::RightParen, "Expected ')' after arguments");

            auto call = std::make_unique<CallExpr>();
            call->location = expr->location;
            call->callee = std::move(expr);
            call->arguments = std::move(args);
            expr = std::move(call);
        } else if (check(TokenType::Dot)) {
            // Member access
            SourceLocation loc = current().location;
            advance();
            // Allow keywords like 'new' as member names
            Token member = (check(TokenType::Identifier) || check(TokenType::KwNew))
                ? advance()
                : expect(TokenType::Identifier, "Expected member name after '.'");

            auto memberExpr = std::make_unique<MemberExpr>();
            memberExpr->location = loc;
            memberExpr->object = std::move(expr);
            memberExpr->member = member.value;
            expr = std::move(memberExpr);
        } else if (check(TokenType::QuestionDot)) {
            // Optional chain: expr?.member
            SourceLocation loc = current().location;
            advance();
            Token member = (check(TokenType::Identifier) || check(TokenType::KwNew))
                ? advance()
                : expect(TokenType::Identifier, "Expected member name after '?.'");

            auto optChain = std::make_unique<OptionalChainExpr>();
            optChain->location = loc;
            optChain->object = std::move(expr);
            optChain->member = member.value;
            expr = std::move(optChain);
        } else if (check(TokenType::LeftBracket)) {
            // Index expression: expr[index]
            SourceLocation loc = current().location;
            advance(); // consume '['
            auto indexExpr = parseExpression();
            expect(TokenType::RightBracket, "Expected ']' after index");

            auto idx = std::make_unique<IndexExpr>();
            idx->location = loc;
            idx->object = std::move(expr);
            idx->index = std::move(indexExpr);
            expr = std::move(idx);
        } else if (check(TokenType::Not)) {
            // Force unwrap: expr!
            // Only treat as postfix if it's immediately after an expression (no whitespace check needed)
            SourceLocation loc = current().location;
            advance();

            auto unwrap = std::make_unique<ForceUnwrapExpr>();
            unwrap->location = loc;
            unwrap->operand = std::move(expr);
            expr = std::move(unwrap);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    skipComments();
    SourceLocation loc = current().location;

    // If-as-expression: if cond { expr } else { expr }
    if (check(TokenType::KwIf)) {
        advance(); // consume 'if'
        auto condition = parseOr(); // parse condition (no parens needed)
        expect(TokenType::LeftBrace, "Expected '{' after if condition");
        auto thenExpr = parseExpression();
        expect(TokenType::RightBrace, "Expected '}' after if expression body");
        expect(TokenType::KwElse, "Expected 'else' in if expression");
        expect(TokenType::LeftBrace, "Expected '{' after else");
        auto elseExpr = parseExpression();
        expect(TokenType::RightBrace, "Expected '}' after else expression body");
        auto ifExpr = std::make_unique<IfExpr>();
        ifExpr->location = loc;
        ifExpr->condition = std::move(condition);
        ifExpr->thenExpr = std::move(thenExpr);
        ifExpr->elseExpr = std::move(elseExpr);
        return ifExpr;
    }

    // Integer literal
    if (check(TokenType::IntLiteral)) {
        int64_t val = std::stoll(current().value);
        advance();
        auto expr = std::make_unique<IntLiteralExpr>();
        expr->location = loc;
        expr->value = val;
        return expr;
    }

    // Float literal
    if (check(TokenType::FloatLiteral)) {
        double val = std::stod(current().value);
        advance();
        auto expr = std::make_unique<FloatLiteralExpr>();
        expr->location = loc;
        expr->value = val;
        return expr;
    }

    // String literal
    if (check(TokenType::StringLiteral)) {
        std::string val = current().value;
        advance();
        auto expr = std::make_unique<StringLiteralExpr>();
        expr->location = loc;
        expr->value = val;
        return expr;
    }

    // String interpolation
    if (check(TokenType::StringInterpStart)) {
        return parseStringInterpolation();
    }

    // Char literal
    if (check(TokenType::CharLiteral)) {
        char val = current().value.empty() ? '\0' : current().value[0];
        advance();
        auto expr = std::make_unique<CharLiteralExpr>();
        expr->location = loc;
        expr->value = val;
        return expr;
    }

    // Bool literal
    if (check(TokenType::BoolLiteral)) {
        bool val = (current().value == "true");
        advance();
        auto expr = std::make_unique<BoolLiteralExpr>();
        expr->location = loc;
        expr->value = val;
        return expr;
    }

    // Nil literal
    if (check(TokenType::NilLiteral)) {
        advance();
        auto expr = std::make_unique<NilLiteralExpr>();
        expr->location = loc;
        return expr;
    }

    // Array literal: [expr, expr, ...]
    if (check(TokenType::LeftBracket)) {
        advance(); // consume '['
        auto arrExpr = std::make_unique<ArrayLiteralExpr>();
        arrExpr->location = loc;
        if (!check(TokenType::RightBracket)) {
            do {
                skipComments();
                arrExpr->elements.push_back(parseExpression());
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightBracket, "Expected ']' after array elements");
        return arrExpr;
    }

    // Match expression
    if (check(TokenType::KwMatch)) {
        advance(); // consume 'match'
        auto subject = parseExpression();

        expect(TokenType::LeftBrace, "Expected '{' after match subject");
        skipComments();

        auto matchExpr = std::make_unique<MatchExpr>();
        matchExpr->location = loc;
        matchExpr->subject = std::move(subject);

        while (!check(TokenType::RightBrace) && !isAtEnd()) {
            skipComments();
            MatchArm arm;

            // Parse pattern: EnumName.CaseName or just CaseName, with optional binding Ok(val)
            Token firstName = expect(TokenType::Identifier, "Expected pattern in match arm");
            if (check(TokenType::Dot)) {
                advance(); // consume '.'
                Token caseName = expect(TokenType::Identifier, "Expected case name after '.'");
                arm.enumName = firstName.value;
                arm.caseName = caseName.value;
            } else {
                arm.caseName = firstName.value;
            }

            // Optional destructuring binding: Ok(val)
            if (match(TokenType::LeftParen)) {
                Token bindName = expect(TokenType::Identifier, "Expected binding name in pattern");
                arm.bindingName = bindName.value;
                expect(TokenType::RightParen, "Expected ')' after binding name");
            }

            expect(TokenType::FatArrow, "Expected '=>' after match pattern");

            // Parse arm body — could be a block or a single expression
            if (check(TokenType::LeftBrace)) {
                arm.body = parseBlock();
            } else {
                auto bodyExpr = parseExpression();
                auto exprStmt = std::make_unique<ExprStmt>();
                exprStmt->location = bodyExpr->location;
                exprStmt->expression = std::move(bodyExpr);
                arm.body = std::move(exprStmt);
                // Consume optional semicolon after expression arm
                match(TokenType::Semicolon);
            }

            matchExpr->arms.push_back(std::move(arm));
            skipComments();
        }

        expect(TokenType::RightBrace, "Expected '}' after match body");
        return matchExpr;
    }

    // 'this' keyword
    if (check(TokenType::KwThis)) {
        advance();
        auto expr = std::make_unique<ThisExpr>();
        expr->location = loc;
        return expr;
    }

    // Identifier, possibly followed by { } for struct construction
    if (check(TokenType::Identifier)) {
        std::string name = current().value;
        advance();

        // Check for struct/class construction: ClassName { field: value, ... }
        if (check(TokenType::LeftBrace)) {
            // Lookahead: is this "Ident { Ident :" pattern? (construct expr)
            // vs just a block. Check if next tokens are "identifier :"
            size_t savedPos = pos_;
            advance(); // consume '{'
            skipComments();
            if (check(TokenType::Identifier)) {
                size_t savedPos2 = pos_;
                advance();
                if (check(TokenType::Colon)) {
                    // This is a construct expression
                    pos_ = savedPos;
                    advance(); // consume '{'
                    skipComments();

                    auto construct = std::make_unique<ConstructExpr>();
                    construct->location = loc;
                    construct->className = name;

                    do {
                        skipComments();
                        Token fieldName = expect(TokenType::Identifier, "Expected field name");
                        expect(TokenType::Colon, "Expected ':' after field name");
                        auto value = parseExpression();
                        construct->fieldInits.push_back({fieldName.value, std::move(value)});
                    } while (match(TokenType::Comma));

                    skipComments();
                    expect(TokenType::RightBrace, "Expected '}' after construction");
                    return construct;
                }
                pos_ = savedPos2;
            }
            pos_ = savedPos;
        }

        auto expr = std::make_unique<IdentifierExpr>();
        expr->location = loc;
        expr->name = name;
        return expr;
    }

    // Lambda or parenthesized expression
    if (check(TokenType::LeftParen)) {
        // Lookahead: is this (params) => ... ?
        size_t savedPos = pos_;
        advance(); // consume '('
        bool isLambda = false;

        // Try to parse as lambda parameter list
        // Lambda params: () => ..., (x) => ..., (x, y) => ..., (x: Int) => ...
        if (check(TokenType::RightParen)) {
            // () => ... — zero-param lambda
            advance();
            if (check(TokenType::FatArrow)) {
                isLambda = true;
            }
        } else {
            // Try scanning ahead: identifiers optionally with : Type, separated by commas, then ) =>
            bool valid = true;
            while (valid && !isAtEnd()) {
                if (!check(TokenType::Identifier)) { valid = false; break; }
                advance(); // consume param name
                if (check(TokenType::Colon)) {
                    advance(); // consume ':'
                    // Skip type (identifier, possibly with ?)
                    if (!check(TokenType::Identifier)) { valid = false; break; }
                    advance();
                    if (check(TokenType::QuestionMark)) advance(); // nullable
                }
                if (check(TokenType::Comma)) {
                    advance();
                } else if (check(TokenType::RightParen)) {
                    advance();
                    if (check(TokenType::FatArrow)) {
                        isLambda = true;
                    }
                    break;
                } else {
                    break;
                }
            }
        }

        pos_ = savedPos; // restore position

        if (isLambda) {
            advance(); // consume '('
            auto lambda = std::make_unique<LambdaExpr>();
            lambda->location = loc;

            // Parse parameters
            if (!check(TokenType::RightParen)) {
                do {
                    LambdaParam param;
                    Token paramName = expect(TokenType::Identifier, "Expected parameter name");
                    param.name = paramName.value;
                    if (match(TokenType::Colon)) {
                        param.type = parseTypeExpr();
                    }
                    lambda->params.push_back(std::move(param));
                } while (match(TokenType::Comma));
            }
            expect(TokenType::RightParen, "Expected ')' after lambda parameters");
            expect(TokenType::FatArrow, "Expected '=>' after lambda parameters");

            // Parse body: block or expression
            if (check(TokenType::LeftBrace)) {
                lambda->bodyBlock = parseBlock();
            } else {
                lambda->bodyExpr = parseExpression();
            }
            return lambda;
        }

        // Regular parenthesized expression
        advance(); // consume '('
        auto expr = parseExpression();
        expect(TokenType::RightParen, "Expected ')' after expression");
        return expr;
    }

    // Error
    diagnostics_.error("E2002",
        "Unexpected token: " + Token::typeToString(current().type) +
        (current().value.empty() ? "" : " '" + current().value + "'"),
        loc);
    advance(); // skip the bad token
    auto expr = std::make_unique<IntLiteralExpr>();
    expr->location = loc;
    expr->value = 0;
    return expr;
}

ExprPtr Parser::parseStringInterpolation() {
    SourceLocation loc = current().location;
    auto interp = std::make_unique<StringInterpolationExpr>();
    interp->location = loc;

    // StringInterpStart contains the first text part
    interp->parts.push_back(current().value);
    advance(); // consume StringInterpStart

    // Now we expect: expression tokens... then StringInterpMiddle or StringInterpEnd
    // The lexer has already structured this for us
    while (true) {
        // Parse the expression
        auto expr = parseExpression();
        interp->expressions.push_back(std::move(expr));

        if (check(TokenType::StringInterpMiddle)) {
            interp->parts.push_back(current().value);
            advance();
            // Continue to next expression
        } else if (check(TokenType::StringInterpEnd)) {
            interp->parts.push_back(current().value);
            advance();
            break;
        } else {
            diagnostics_.error("E2003", "Expected string interpolation continuation", current().location);
            interp->parts.push_back("");
            break;
        }
    }

    return interp;
}

TypeExprPtr Parser::parseTypeExpr() {
    // Array shorthand: [T] -> Array<T>
    if (check(TokenType::LeftBracket)) {
        advance(); // consume '['
        auto elemType = parseTypeExpr();
        expect(TokenType::RightBracket, "Expected ']' after array element type");
        auto arrayType = std::make_unique<NamedType>();
        arrayType->location = elemType->location;
        arrayType->name = "Array";
        arrayType->typeArgs.push_back(std::move(elemType));
        // Check for nullable: [Int]?
        if (check(TokenType::QuestionMark)) {
            advance();
            arrayType->nullable = true;
        }
        return arrayType;
    }

    // Function type: (ParamType, ...) -> ReturnType
    if (check(TokenType::LeftParen)) {
        // Save position to backtrack if not a function type
        size_t savedPos = pos_;
        advance(); // consume '('

        std::vector<TypeExprPtr> paramTypes;
        if (!check(TokenType::RightParen)) {
            // Try parsing as type list
            bool isTypeList = true;
            paramTypes.push_back(parseTypeExpr());
            while (match(TokenType::Comma)) {
                paramTypes.push_back(parseTypeExpr());
            }
            if (!check(TokenType::RightParen)) {
                isTypeList = false;
            }
            if (isTypeList) {
                advance(); // consume ')'
                if (check(TokenType::Arrow)) {
                    advance(); // consume '->'
                    auto returnType = parseTypeExpr();
                    // Build a FunctionType representation using NamedType convention
                    // Store as NamedType with name "__func" and typeArgs = [param1, param2, ..., returnType]
                    auto funcType = std::make_unique<NamedType>();
                    funcType->location = paramTypes.empty() ? returnType->location : paramTypes[0]->location;
                    funcType->name = "__func";
                    funcType->nullable = false;
                    for (auto& pt : paramTypes) {
                        funcType->typeArgs.push_back(std::move(pt));
                    }
                    funcType->typeArgs.push_back(std::move(returnType));
                    return funcType;
                }
                // Not a function type — backtrack
                pos_ = savedPos;
            } else {
                pos_ = savedPos;
            }
        } else {
            advance(); // consume ')'
            if (check(TokenType::Arrow)) {
                advance(); // consume '->'
                auto returnType = parseTypeExpr();
                auto funcType = std::make_unique<NamedType>();
                funcType->location = returnType->location;
                funcType->name = "__func";
                funcType->nullable = false;
                funcType->typeArgs.push_back(std::move(returnType));
                return funcType;
            }
            // Not a function type — backtrack
            pos_ = savedPos;
        }
    }

    Token name = expect(TokenType::Identifier, "Expected type name");

    auto type = std::make_unique<NamedType>();
    type->location = name.location;
    type->name = name.value;
    type->nullable = false;

    // Parse optional generic type arguments: Box<Int> or Pair<Int, String>
    if (check(TokenType::Less)) {
        advance(); // consume '<'
        do {
            type->typeArgs.push_back(parseTypeExpr());
        } while (match(TokenType::Comma));
        expect(TokenType::Greater, "Expected '>' after type arguments");
    }

    if (match(TokenType::QuestionMark)) {
        type->nullable = true;
    }

    return type;
}

std::vector<ExprPtr> Parser::parseArguments() {
    std::vector<ExprPtr> args;
    if (!check(TokenType::RightParen)) {
        do {
            args.push_back(parseExpression());
        } while (match(TokenType::Comma));
    }
    return args;
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (tokens_[pos_ - 1].type == TokenType::Semicolon) return;

        switch (current().type) {
            case TokenType::KwFunc:
            case TokenType::KwVar:
            case TokenType::KwLet:
            case TokenType::KwIf:
            case TokenType::KwWhile:
            case TokenType::KwFor:
            case TokenType::KwReturn:
            case TokenType::KwImport:
            case TokenType::KwClass:
                return;
            default:
                advance();
        }
    }
}

} // namespace chris
