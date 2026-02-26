#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace chris {

enum class TypeKind {
    Int,
    Int8,
    Int16,
    Int32,
    UInt,
    UInt8,
    UInt16,
    UInt32,
    Float,
    Float32,
    Bool,
    String,
    Char,
    Void,
    Nil,
    Nullable,   // wraps another type
    Function,
    Class,
    Enum,
    TypeParameter, // generic type parameter (e.g. T)
    Array,
    Future,
    Map,
    Set,
    TypeInfo,
    Unknown
};

struct Type {
    virtual ~Type() = default;
    virtual TypeKind kind() const = 0;
    virtual std::string toString() const = 0;
    virtual bool equals(const Type& other) const = 0;
    bool isNumeric() const {
        auto k = kind();
        return k == TypeKind::Int || k == TypeKind::Float ||
               k == TypeKind::Int8 || k == TypeKind::Int16 || k == TypeKind::Int32 ||
               k == TypeKind::UInt || k == TypeKind::UInt8 || k == TypeKind::UInt16 || k == TypeKind::UInt32 ||
               k == TypeKind::Float32;
    }
    bool isNullable() const { return kind() == TypeKind::Nullable; }
};

using TypePtr = std::shared_ptr<Type>;

struct PrimitiveType : Type {
    TypeKind kind_;
    PrimitiveType(TypeKind k) : kind_(k) {}
    TypeKind kind() const override { return kind_; }
    std::string toString() const override;
    bool equals(const Type& other) const override {
        return other.kind() == kind_;
    }
};

struct NullableType : Type {
    TypePtr inner;
    NullableType(TypePtr inner) : inner(std::move(inner)) {}
    TypeKind kind() const override { return TypeKind::Nullable; }
    std::string toString() const override { return inner->toString() + "?"; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Nullable) return false;
        auto& otherNullable = static_cast<const NullableType&>(other);
        return inner->equals(*otherNullable.inner);
    }
};

struct FunctionType : Type {
    std::vector<TypePtr> paramTypes;
    TypePtr returnType;
    TypeKind kind() const override { return TypeKind::Function; }
    std::string toString() const override;
    bool equals(const Type& other) const override;
};

enum class AccessLevel { Private, Protected, Public };

struct ClassField {
    std::string name;
    TypePtr type;
    bool isPublic; // kept for backward compat
    AccessLevel access = AccessLevel::Private;
};

struct ClassMethod {
    std::string name;
    TypePtr type; // FunctionType
    bool isPublic; // kept for backward compat
    AccessLevel access = AccessLevel::Private;
};

struct ClassType : Type {
    std::string name;
    bool isShared = false; // true for shared classes (thread-safe synchronized access)
    std::shared_ptr<ClassType> parent; // base class (nullptr if none)
    std::vector<std::string> interfaceNames; // implemented interfaces
    std::vector<ClassField> fields;
    std::vector<ClassMethod> methods;
    std::vector<std::string> typeParams; // generic type parameter names (e.g. ["T", "U"])
    std::vector<TypePtr> typeArgs;       // concrete type arguments for instantiated generics
    bool isGenericTemplate() const { return !typeParams.empty() && typeArgs.empty(); }
    bool isGenericInstance() const { return !typeArgs.empty(); }
    TypeKind kind() const override { return TypeKind::Class; }
    std::string toString() const override {
        std::string result = name;
        if (!typeArgs.empty()) {
            result += "<";
            for (size_t i = 0; i < typeArgs.size(); i++) {
                if (i > 0) result += ", ";
                result += typeArgs[i]->toString();
            }
            result += ">";
        }
        return result;
    }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Class) return false;
        auto& otherClass = static_cast<const ClassType&>(other);
        if (name != otherClass.name) return false;
        if (typeArgs.size() != otherClass.typeArgs.size()) return false;
        for (size_t i = 0; i < typeArgs.size(); i++) {
            if (!typeArgs[i]->equals(*otherClass.typeArgs[i])) return false;
        }
        return true;
    }
    TypePtr getFieldType(const std::string& fieldName) const;
    TypePtr getMethodType(const std::string& methodName) const;
    bool isSubclassOf(const std::string& className) const;
};

struct ArrayType : Type {
    TypePtr elementType;
    ArrayType(TypePtr elem) : elementType(std::move(elem)) {}
    TypeKind kind() const override { return TypeKind::Array; }
    std::string toString() const override { return "[" + elementType->toString() + "]"; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Array) return false;
        return elementType->equals(*static_cast<const ArrayType&>(other).elementType);
    }
};

struct FutureType : Type {
    TypePtr innerType;
    FutureType(TypePtr inner) : innerType(std::move(inner)) {}
    TypeKind kind() const override { return TypeKind::Future; }
    std::string toString() const override { return "Future<" + innerType->toString() + ">"; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Future) return false;
        return innerType->equals(*static_cast<const FutureType&>(other).innerType);
    }
};

struct MapType : Type {
    TypePtr keyType;
    TypePtr valueType;
    MapType(TypePtr key, TypePtr val) : keyType(std::move(key)), valueType(std::move(val)) {}
    TypeKind kind() const override { return TypeKind::Map; }
    std::string toString() const override { return "Map<" + keyType->toString() + ", " + valueType->toString() + ">"; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Map) return false;
        auto& otherMap = static_cast<const MapType&>(other);
        return keyType->equals(*otherMap.keyType) && valueType->equals(*otherMap.valueType);
    }
};

struct SetType : Type {
    TypePtr elementType;
    SetType(TypePtr elem) : elementType(std::move(elem)) {}
    TypeKind kind() const override { return TypeKind::Set; }
    std::string toString() const override { return "Set<" + elementType->toString() + ">"; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Set) return false;
        return elementType->equals(*static_cast<const SetType&>(other).elementType);
    }
};

struct TypeInfoType : Type {
    TypeKind kind() const override { return TypeKind::TypeInfo; }
    std::string toString() const override { return "TypeInfo"; }
    bool equals(const Type& other) const override {
        return other.kind() == TypeKind::TypeInfo;
    }
};

struct TypeParameterType : Type {
    std::string name; // e.g. "T"
    TypeKind kind() const override { return TypeKind::TypeParameter; }
    std::string toString() const override { return name; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::TypeParameter) return false;
        return name == static_cast<const TypeParameterType&>(other).name;
    }
};

struct InterfaceType : Type {
    std::string name;
    std::vector<ClassMethod> methods;
    TypeKind kind() const override { return TypeKind::Class; } // interfaces are class-like for assignability
    std::string toString() const override { return name; }
    bool equals(const Type& other) const override {
        auto* other_iface = dynamic_cast<const InterfaceType*>(&other);
        if (other_iface) return name == other_iface->name;
        return false;
    }
};

struct EnumType : Type {
    std::string name;
    std::vector<std::string> cases;
    std::unordered_map<std::string, TypePtr> associatedTypes; // case name -> associated value type (nullptr = none)
    TypeKind kind() const override { return TypeKind::Enum; }
    std::string toString() const override { return name; }
    bool equals(const Type& other) const override {
        if (other.kind() != TypeKind::Enum) return false;
        return name == static_cast<const EnumType&>(other).name;
    }
    int getCaseIndex(const std::string& caseName) const {
        for (size_t i = 0; i < cases.size(); i++) {
            if (cases[i] == caseName) return static_cast<int>(i);
        }
        return -1;
    }
    bool hasAssociatedValue(const std::string& caseName) const {
        auto it = associatedTypes.find(caseName);
        return it != associatedTypes.end() && it->second != nullptr;
    }
};

// Singleton accessors for built-in types
TypePtr intType();
TypePtr int8Type();
TypePtr int16Type();
TypePtr int32Type();
TypePtr uintType();
TypePtr uint8Type();
TypePtr uint16Type();
TypePtr uint32Type();
TypePtr floatType();
TypePtr float32Type();
TypePtr boolType();
TypePtr stringType();
TypePtr charType();
TypePtr voidType();
TypePtr nilType();
TypePtr unknownType();
TypePtr makeNullable(TypePtr inner);
TypePtr makeFunctionType(std::vector<TypePtr> params, TypePtr ret);
TypePtr makeClassType(const std::string& name);
TypePtr makeTypeParameter(const std::string& name);
TypePtr makeArrayType(TypePtr elementType);
TypePtr makeFutureType(TypePtr innerType);
TypePtr makeMapType(TypePtr keyType, TypePtr valueType);
TypePtr makeSetType(TypePtr elementType);
TypePtr typeInfoType();

// Substitute type parameters with concrete types in a given type
TypePtr substituteTypeParams(
    const TypePtr& type,
    const std::vector<std::string>& paramNames,
    const std::vector<TypePtr>& args);

// Resolve a type name string to a Type
TypePtr resolveTypeName(const std::string& name);

// Check if a value type is assignable to a target type
bool isAssignable(const TypePtr& target, const TypePtr& value);

struct GenericInstantiation {
    std::string templateName;
    std::string mangledName;
    std::vector<std::string> typeParams;
    std::vector<TypePtr> typeArgs;
};

} // namespace chris
