#include "sema/types.h"
#include <sstream>

namespace chris {

std::string PrimitiveType::toString() const {
    switch (kind_) {
        case TypeKind::Int:     return "Int";
        case TypeKind::Int8:    return "Int8";
        case TypeKind::Int16:   return "Int16";
        case TypeKind::Int32:   return "Int32";
        case TypeKind::UInt:    return "UInt";
        case TypeKind::UInt8:   return "UInt8";
        case TypeKind::UInt16:  return "UInt16";
        case TypeKind::UInt32:  return "UInt32";
        case TypeKind::Float:   return "Float";
        case TypeKind::Float32: return "Float32";
        case TypeKind::Bool:    return "Bool";
        case TypeKind::String:  return "String";
        case TypeKind::Char:    return "Char";
        case TypeKind::Void:    return "Void";
        case TypeKind::Nil:     return "nil";
        case TypeKind::Unknown: return "<unknown>";
        default:                return "<type>";
    }
}

std::string FunctionType::toString() const {
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) oss << ", ";
        oss << paramTypes[i]->toString();
    }
    oss << ") -> " << returnType->toString();
    return oss.str();
}

bool FunctionType::equals(const Type& other) const {
    if (other.kind() != TypeKind::Function) return false;
    auto& otherFunc = static_cast<const FunctionType&>(other);
    if (paramTypes.size() != otherFunc.paramTypes.size()) return false;
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (!paramTypes[i]->equals(*otherFunc.paramTypes[i])) return false;
    }
    return returnType->equals(*otherFunc.returnType);
}

// Singletons
static TypePtr intType_ = std::make_shared<PrimitiveType>(TypeKind::Int);
static TypePtr int8Type_ = std::make_shared<PrimitiveType>(TypeKind::Int8);
static TypePtr int16Type_ = std::make_shared<PrimitiveType>(TypeKind::Int16);
static TypePtr int32Type_ = std::make_shared<PrimitiveType>(TypeKind::Int32);
static TypePtr uintType_ = std::make_shared<PrimitiveType>(TypeKind::UInt);
static TypePtr uint8Type_ = std::make_shared<PrimitiveType>(TypeKind::UInt8);
static TypePtr uint16Type_ = std::make_shared<PrimitiveType>(TypeKind::UInt16);
static TypePtr uint32Type_ = std::make_shared<PrimitiveType>(TypeKind::UInt32);
static TypePtr floatType_ = std::make_shared<PrimitiveType>(TypeKind::Float);
static TypePtr float32Type_ = std::make_shared<PrimitiveType>(TypeKind::Float32);
static TypePtr boolType_ = std::make_shared<PrimitiveType>(TypeKind::Bool);
static TypePtr stringType_ = std::make_shared<PrimitiveType>(TypeKind::String);
static TypePtr charType_ = std::make_shared<PrimitiveType>(TypeKind::Char);
static TypePtr voidType_ = std::make_shared<PrimitiveType>(TypeKind::Void);
static TypePtr nilType_ = std::make_shared<PrimitiveType>(TypeKind::Nil);
static TypePtr unknownType_ = std::make_shared<PrimitiveType>(TypeKind::Unknown);

TypePtr intType()     { return intType_; }
TypePtr int8Type()    { return int8Type_; }
TypePtr int16Type()   { return int16Type_; }
TypePtr int32Type()   { return int32Type_; }
TypePtr uintType()    { return uintType_; }
TypePtr uint8Type()   { return uint8Type_; }
TypePtr uint16Type()  { return uint16Type_; }
TypePtr uint32Type()  { return uint32Type_; }
TypePtr floatType()   { return floatType_; }
TypePtr float32Type() { return float32Type_; }
TypePtr boolType()    { return boolType_; }
TypePtr stringType()  { return stringType_; }
TypePtr charType()    { return charType_; }
TypePtr voidType()    { return voidType_; }
TypePtr nilType()     { return nilType_; }
TypePtr unknownType() { return unknownType_; }

TypePtr makeNullable(TypePtr inner) {
    return std::make_shared<NullableType>(std::move(inner));
}

TypePtr makeFunctionType(std::vector<TypePtr> params, TypePtr ret) {
    auto ft = std::make_shared<FunctionType>();
    ft->paramTypes = std::move(params);
    ft->returnType = std::move(ret);
    return ft;
}

TypePtr makeClassType(const std::string& name) {
    auto ct = std::make_shared<ClassType>();
    ct->name = name;
    return ct;
}

TypePtr makeTypeParameter(const std::string& name) {
    auto tp = std::make_shared<TypeParameterType>();
    tp->name = name;
    return tp;
}

TypePtr makeArrayType(TypePtr elementType) {
    return std::make_shared<ArrayType>(std::move(elementType));
}

TypePtr makeFutureType(TypePtr innerType) {
    return std::make_shared<FutureType>(std::move(innerType));
}

TypePtr makeMapType(TypePtr keyType, TypePtr valueType) {
    return std::make_shared<MapType>(std::move(keyType), std::move(valueType));
}

TypePtr makeSetType(TypePtr elementType) {
    return std::make_shared<SetType>(std::move(elementType));
}

TypePtr typeInfoType() {
    return std::make_shared<TypeInfoType>();
}

TypePtr substituteTypeParams(
    const TypePtr& type,
    const std::vector<std::string>& paramNames,
    const std::vector<TypePtr>& args)
{
    if (!type) return type;

    // If it's a type parameter, substitute it
    if (type->kind() == TypeKind::TypeParameter) {
        auto& tp = static_cast<const TypeParameterType&>(*type);
        for (size_t i = 0; i < paramNames.size(); i++) {
            if (paramNames[i] == tp.name && i < args.size()) {
                return args[i];
            }
        }
        return type; // unresolved type parameter
    }

    // Recurse into nullable
    if (type->kind() == TypeKind::Nullable) {
        auto& nullable = static_cast<const NullableType&>(*type);
        auto inner = substituteTypeParams(nullable.inner, paramNames, args);
        if (inner != nullable.inner) return makeNullable(inner);
        return type;
    }

    // Recurse into function types
    if (type->kind() == TypeKind::Function) {
        auto& func = static_cast<const FunctionType&>(*type);
        std::vector<TypePtr> newParams;
        bool changed = false;
        for (auto& p : func.paramTypes) {
            auto np = substituteTypeParams(p, paramNames, args);
            if (np != p) changed = true;
            newParams.push_back(np);
        }
        auto newRet = substituteTypeParams(func.returnType, paramNames, args);
        if (newRet != func.returnType) changed = true;
        if (changed) return makeFunctionType(std::move(newParams), newRet);
        return type;
    }

    // Recurse into class types (for nested generics)
    if (type->kind() == TypeKind::Class) {
        auto& cls = static_cast<const ClassType&>(*type);
        if (!cls.typeArgs.empty()) {
            std::vector<TypePtr> newArgs;
            bool changed = false;
            for (auto& a : cls.typeArgs) {
                auto na = substituteTypeParams(a, paramNames, args);
                if (na != a) changed = true;
                newArgs.push_back(na);
            }
            if (changed) {
                auto newCls = std::make_shared<ClassType>();
                newCls->name = cls.name;
                newCls->typeParams = cls.typeParams;
                newCls->typeArgs = std::move(newArgs);
                newCls->fields = cls.fields;
                newCls->methods = cls.methods;
                newCls->parent = cls.parent;
                newCls->interfaceNames = cls.interfaceNames;
                return newCls;
            }
        }
        return type;
    }

    return type;
}

TypePtr ClassType::getFieldType(const std::string& fieldName) const {
    for (auto& f : fields) {
        if (f.name == fieldName) return f.type;
    }
    // Check parent class
    if (parent) return parent->getFieldType(fieldName);
    return nullptr;
}

TypePtr ClassType::getMethodType(const std::string& methodName) const {
    for (auto& m : methods) {
        if (m.name == methodName) return m.type;
    }
    // Check parent class
    if (parent) return parent->getMethodType(methodName);
    return nullptr;
}

bool ClassType::isSubclassOf(const std::string& className) const {
    if (name == className) return true;
    if (parent) return parent->isSubclassOf(className);
    return false;
}

TypePtr resolveTypeName(const std::string& name) {
    if (name == "Int")    return intType();
    if (name == "Int8")   return int8Type();
    if (name == "Int16")  return int16Type();
    if (name == "Int32")  return int32Type();
    if (name == "UInt")   return uintType();
    if (name == "UInt8")  return uint8Type();
    if (name == "UInt16") return uint16Type();
    if (name == "UInt32") return uint32Type();
    if (name == "Float")  return floatType();
    if (name == "Float32") return float32Type();
    if (name == "Bool")   return boolType();
    if (name == "String") return stringType();
    if (name == "Char")   return charType();
    if (name == "Void")   return voidType();
    return nullptr;
}

bool isAssignable(const TypePtr& target, const TypePtr& value) {
    if (!target || !value) return false;

    // Unknown type acts as wildcard (e.g. print accepts any type)
    if (target->kind() == TypeKind::Unknown || value->kind() == TypeKind::Unknown) return true;

    // Type parameters act as wildcards (checked at instantiation)
    if (target->kind() == TypeKind::TypeParameter || value->kind() == TypeKind::TypeParameter) return true;

    // Same type
    if (target->equals(*value)) return true;

    // nil is assignable to nullable types
    if (value->kind() == TypeKind::Nil && target->isNullable()) return true;

    // A non-nullable T is assignable to T?
    if (target->isNullable()) {
        auto& nullable = static_cast<const NullableType&>(*target);
        if (nullable.inner->equals(*value)) return true;
    }

    // Int is promotable to Float
    if (target->kind() == TypeKind::Float && value->kind() == TypeKind::Int) return true;

    // Int literal (Int) is assignable to any integer type
    if (value->kind() == TypeKind::Int) {
        auto tk = target->kind();
        if (tk == TypeKind::Int8 || tk == TypeKind::Int16 || tk == TypeKind::Int32 ||
            tk == TypeKind::UInt || tk == TypeKind::UInt8 || tk == TypeKind::UInt16 || tk == TypeKind::UInt32) {
            return true;
        }
    }

    // Float literal (Float) is assignable to Float32
    if (value->kind() == TypeKind::Float && target->kind() == TypeKind::Float32) return true;

    // Sized integers are promotable to larger sizes and to Float/Float32
    // Int8/Int16/Int32 -> Int
    if (target->kind() == TypeKind::Int) {
        auto vk = value->kind();
        if (vk == TypeKind::Int8 || vk == TypeKind::Int16 || vk == TypeKind::Int32 ||
            vk == TypeKind::UInt8 || vk == TypeKind::UInt16 || vk == TypeKind::UInt32 || vk == TypeKind::UInt) {
            return true;
        }
    }

    // Subclass is assignable to parent class
    if (target->kind() == TypeKind::Class && value->kind() == TypeKind::Class) {
        auto* valueClass = dynamic_cast<const ClassType*>(value.get());
        auto* targetClass = dynamic_cast<const ClassType*>(target.get());
        if (valueClass && targetClass) {
            if (valueClass->isSubclassOf(targetClass->name)) return true;
        }
        // Class implementing an interface is assignable to that interface
        auto* targetIface = dynamic_cast<const InterfaceType*>(target.get());
        if (valueClass && targetIface) {
            for (auto& ifaceName : valueClass->interfaceNames) {
                if (ifaceName == targetIface->name) return true;
            }
            // Check parent chain for interface implementation
            auto parent = valueClass->parent;
            while (parent) {
                for (auto& ifaceName : parent->interfaceNames) {
                    if (ifaceName == targetIface->name) return true;
                }
                parent = parent->parent;
            }
        }
    }

    return false;
}

} // namespace chris
