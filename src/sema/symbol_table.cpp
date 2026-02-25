#include "sema/symbol_table.h"

namespace chris {

bool Scope::define(const std::string& name, TypePtr type, bool isMutable, const SourceLocation& loc) {
    if (symbols_.count(name)) return false; // already defined in this scope
    symbols_[name] = Symbol{name, std::move(type), isMutable, loc};
    return true;
}

Symbol* Scope::lookup(const std::string& name) {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) return &it->second;
    if (parent_) return parent_->lookup(name);
    return nullptr;
}

Symbol* Scope::lookupLocal(const std::string& name) {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) return &it->second;
    return nullptr;
}

SymbolTable::SymbolTable() : current_(std::make_shared<Scope>()) {}

void SymbolTable::pushScope() {
    current_ = std::make_shared<Scope>(current_);
}

void SymbolTable::popScope() {
    if (current_->parent()) {
        current_ = current_->parent();
    }
}

bool SymbolTable::define(const std::string& name, TypePtr type, bool isMutable, const SourceLocation& loc) {
    return current_->define(name, std::move(type), isMutable, loc);
}

Symbol* SymbolTable::lookup(const std::string& name) {
    return current_->lookup(name);
}

Symbol* SymbolTable::lookupLocal(const std::string& name) {
    return current_->lookupLocal(name);
}

} // namespace chris
