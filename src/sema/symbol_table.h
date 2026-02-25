#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "sema/types.h"
#include "common/source_location.h"

namespace chris {

struct Symbol {
    std::string name;
    TypePtr type;
    bool isMutable;
    SourceLocation location;
};

class Scope {
public:
    Scope(std::shared_ptr<Scope> parent = nullptr) : parent_(parent) {}

    bool define(const std::string& name, TypePtr type, bool isMutable, const SourceLocation& loc);
    Symbol* lookup(const std::string& name);
    Symbol* lookupLocal(const std::string& name);
    std::shared_ptr<Scope> parent() const { return parent_; }

private:
    std::shared_ptr<Scope> parent_;
    std::unordered_map<std::string, Symbol> symbols_;
};

class SymbolTable {
public:
    SymbolTable();

    void pushScope();
    void popScope();
    bool define(const std::string& name, TypePtr type, bool isMutable, const SourceLocation& loc);
    Symbol* lookup(const std::string& name);
    Symbol* lookupLocal(const std::string& name);
    std::shared_ptr<Scope> currentScope() const { return current_; }

private:
    std::shared_ptr<Scope> current_;
};

} // namespace chris
