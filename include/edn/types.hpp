// Basic type system scaffolding for Phase 1.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <tuple>
#include <stdexcept>
#include "edn/edn.hpp"

namespace edn
{

    using TypeId = uint32_t;

    // Phase 2: add unsigned integer base types (same bit widths; semantics differ in ops)
    enum class BaseType
    {
        I1,
        I8,
        I16,
        I32,
        I64,
        U8,
        U16,
        U32,
        U64,
        F32,
        F64,
        Void
    };

    struct Type
    {
        enum class Kind
        {
            Base,
            Pointer,
            Struct,
            Function,
            Array
        } kind;
        BaseType base{};            // Base
        TypeId pointee{0};          // Pointer
        std::string struct_name;    // Struct
        std::vector<TypeId> params; // Function
        TypeId ret{0};              // Function
        bool variadic{false};       // Function
        TypeId elem{0};             // Array
        uint64_t array_size{0};     // Array
    };

    class TypeContext
    {
    public:
        TypeContext()
        { // seed base types (order matters only for stable ids across run)
            get_base(BaseType::I1);
            get_base(BaseType::I8);
            get_base(BaseType::I16);
            get_base(BaseType::I32);
            get_base(BaseType::I64);
            get_base(BaseType::U8);
            get_base(BaseType::U16);
            get_base(BaseType::U32);
            get_base(BaseType::U64);
            get_base(BaseType::F32);
            get_base(BaseType::F64);
            get_base(BaseType::Void);
        }

        TypeId get_base(BaseType b)
        {
            auto key = static_cast<int>(b);
            auto it = base_index_.find(key);
            if (it != base_index_.end())
                return it->second;
            // Avoid -Wmissing-field-initializers by explicit construction
            Type t{};
            t.kind = Type::Kind::Base;
            t.base = b;
            TypeId id = add_type(std::move(t));
            base_index_[key] = id;
            return id;
        }
        TypeId get_pointer(TypeId to)
        {
            auto it = ptr_cache_.find(to);
            if (it != ptr_cache_.end())
                return it->second;
            Type t;
            t.kind = Type::Kind::Pointer;
            t.pointee = to;
            TypeId id = add_type(t);
            ptr_cache_[to] = id;
            return id;
        }
        TypeId get_struct(const std::string &name)
        {
            auto it = struct_cache_.find(name);
            if (it != struct_cache_.end())
                return it->second;
            Type t;
            t.kind = Type::Kind::Struct;
            t.struct_name = name;
            TypeId id = add_type(t);
            struct_cache_[name] = id;
            return id;
        }
        TypeId get_function(const std::vector<TypeId> &params, TypeId ret, bool variadic = false)
        {
            auto key = std::make_tuple(params, ret, variadic);
            auto it = fn_cache_.find(key);
            if (it != fn_cache_.end())
                return it->second;
            Type t;
            t.kind = Type::Kind::Function;
            t.params = params;
            t.ret = ret;
            t.variadic = variadic;
            TypeId id = add_type(t);
            fn_cache_[key] = id;
            return id;
        }
        TypeId get_array(TypeId elem, uint64_t size)
        {
            auto key = std::make_pair(elem, size);
            auto it = array_cache_.find(key);
            if (it != array_cache_.end())
                return it->second;
            Type t;
            t.kind = Type::Kind::Array;
            t.elem = elem;
            t.array_size = size;
            TypeId id = add_type(t);
            array_cache_[key] = id;
            return id;
        }

        const Type &at(TypeId id) const { return types_.at(id); }
        std::string to_string(TypeId id) const
        {
            const Type &t = at(id);
            switch (t.kind)
            {
            case Type::Kind::Base:
                return base_name(t.base);
            case Type::Kind::Pointer:
                return to_string(t.pointee) + "*";
            case Type::Kind::Struct:
                return "%struct." + t.struct_name; // simple convention
            case Type::Kind::Function:
            {
                std::string s = to_string(t.ret) + " (";
                for (size_t i = 0; i < t.params.size(); ++i)
                {
                    if (i)
                        s += ", ";
                    s += to_string(t.params[i]);
                }
                if (t.variadic)
                {
                    if (!t.params.empty())
                        s += ", ";
                    s += "...";
                }
                s += ")";
                return s;
            }
            case Type::Kind::Array:
            {
                return "[" + std::to_string(t.array_size) + " x " + to_string(t.elem) + "]";
            }
            }
            return "<bad-type>";
        }

        // Parse an EDN type form -> TypeId
        TypeId parse_type(const node_ptr &n)
        {
            // Symbol for base types or struct ref maybe
            if (std::holds_alternative<symbol>(n->data))
            {
                auto name = std::get<symbol>(n->data).name;
                if (name == "i1")
                    return get_base(BaseType::I1);
                if (name == "i8")
                    return get_base(BaseType::I8);
                if (name == "i16")
                    return get_base(BaseType::I16);
                if (name == "i32")
                    return get_base(BaseType::I32);
                if (name == "i64")
                    return get_base(BaseType::I64);
                if (name == "u8")
                    return get_base(BaseType::U8);
                if (name == "u16")
                    return get_base(BaseType::U16);
                if (name == "u32")
                    return get_base(BaseType::U32);
                if (name == "u64")
                    return get_base(BaseType::U64);
                if (name == "f32")
                    return get_base(BaseType::F32);
                if (name == "f64")
                    return get_base(BaseType::F64);
                if (name == "void")
                    return get_base(BaseType::Void);
                // Fallback: treat as named struct ref
                return get_struct(name);
            }
            if (std::holds_alternative<list>(n->data))
            {
                auto &l = std::get<list>(n->data).elems;
                if (l.empty())
                    throw parse_error("empty type list");
                if (!std::holds_alternative<symbol>(l[0]->data))
                    throw parse_error("type head must be symbol");
                std::string head = std::get<symbol>(l[0]->data).name;
                if (head == "ptr")
                {
                    // (ptr <type>) or (ptr :to <type>)
                    if (l.size() == 2)
                        return get_pointer(parse_type(l[1]));
                    if (l.size() == 3 && std::holds_alternative<keyword>(l[1]->data) && std::get<keyword>(l[1]->data).name == "to")
                        return get_pointer(parse_type(l[2]));
                    throw parse_error("ptr form invalid");
                }
                if (head == "struct-ref")
                {
                    if (l.size() != 2 || !std::holds_alternative<symbol>(l[1]->data))
                        throw parse_error("struct-ref requires name symbol");
                    return get_struct(std::get<symbol>(l[1]->data).name);
                }
                if (head == "fn-type")
                {
                    // (fn-type :params [<types>*] :ret <type> [:variadic true]?)
                    std::vector<TypeId> params;
                    TypeId ret = get_base(BaseType::Void);
                    bool variadic = false;
                    for (size_t i = 1; i < l.size(); ++i)
                    {
                        if (!std::holds_alternative<keyword>(l[i]->data))
                            throw parse_error("expected keyword in fn-type");
                        std::string kw = std::get<keyword>(l[i]->data).name;
                        ++i;
                        if (i >= l.size())
                            throw parse_error("keyword missing value");
                        auto val = l[i];
                        if (kw == "params")
                        {
                            if (!std::holds_alternative<vector_t>(val->data))
                                throw parse_error("fn-type :params expects vector");
                            for (auto &p : std::get<vector_t>(val->data).elems)
                                params.push_back(parse_type(p));
                        }
                        else if (kw == "ret")
                            ret = parse_type(val);
                        else if (kw == "variadic")
                        {
                            if (!std::holds_alternative<bool>(val->data))
                                throw parse_error(":variadic expects bool");
                            variadic = std::get<bool>(val->data);
                        }
                        else
                        {
                            throw parse_error("unknown fn-type keyword");
                        }
                    }
                    return get_function(params, ret, variadic);
                }
                if (head == "array")
                {
                    // (array :elem <type> :size <int>)
                    TypeId elem_id = 0;
                    uint64_t sz = 0;
                    bool have_elem = false, have_size = false;
                    for (size_t i2 = 1; i2 < l.size(); ++i2)
                    {
                        if (!std::holds_alternative<keyword>(l[i2]->data))
                            throw parse_error("expected keyword in array");
                        std::string kw = std::get<keyword>(l[i2]->data).name;
                        ++i2;
                        if (i2 >= l.size())
                            throw parse_error("array keyword missing value");
                        auto val = l[i2];
                        if (kw == "elem")
                        {
                            elem_id = parse_type(val);
                            have_elem = true;
                        }
                        else if (kw == "size")
                        {
                            if (!std::holds_alternative<int64_t>(val->data))
                                throw parse_error("array :size expects int");
                            sz = (uint64_t)std::get<int64_t>(val->data);
                            have_size = true;
                        }
                        else
                            throw parse_error("unknown array keyword");
                    }
                    if (!have_elem || !have_size)
                        throw parse_error("array requires :elem and :size");
                    return get_array(elem_id, sz);
                }
                throw parse_error("unknown type form: " + head);
            }
            throw parse_error("unsupported type node");
        }

    private:
        struct FnKeyHash
        {
            size_t operator()(const std::tuple<std::vector<TypeId>, TypeId, bool> &k) const noexcept
            {
                size_t h = std::hash<TypeId>{}(std::get<1>(k)) ^ (std::get<2>(k) ? 0x9e3779b97f4a7c15ull : 0);
                for (auto t : std::get<0>(k))
                    h ^= (std::hash<TypeId>{}(t) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
                return h;
            }
        };
        struct FnKeyEq
        {
            bool operator()(const std::tuple<std::vector<TypeId>, TypeId, bool> &a,
                            const std::tuple<std::vector<TypeId>, TypeId, bool> &b) const noexcept
            {
                return std::get<1>(a) == std::get<1>(b) && std::get<2>(a) == std::get<2>(b) && std::get<0>(a) == std::get<0>(b);
            }
        };

        TypeId add_type(Type t)
        {
            TypeId id = static_cast<TypeId>(types_.size());
            types_.push_back(std::move(t));
            return id;
        }
        std::string base_name(BaseType b) const
        {
            switch (b)
            {
            case BaseType::I1:
                return "i1";
            case BaseType::I8:
                return "i8";
            case BaseType::I16:
                return "i16";
            case BaseType::I32:
                return "i32";
            case BaseType::I64:
                return "i64";
            case BaseType::U8:
                return "u8";
            case BaseType::U16:
                return "u16";
            case BaseType::U32:
                return "u32";
            case BaseType::U64:
                return "u64";
            case BaseType::F32:
                return "f32";
            case BaseType::F64:
                return "f64";
            case BaseType::Void:
                return "void";
            }
            return "?";
        }

        std::vector<Type> types_;
        std::unordered_map<int, TypeId> base_index_;
        std::unordered_map<TypeId, TypeId> ptr_cache_;
        std::unordered_map<std::string, TypeId> struct_cache_;
        std::unordered_map<std::tuple<std::vector<TypeId>, TypeId, bool>, TypeId, FnKeyHash, FnKeyEq> fn_cache_;
        struct PairHash
        {
            size_t operator()(const std::pair<TypeId, uint64_t> &p) const noexcept { return std::hash<TypeId>{}(p.first) ^ (std::hash<uint64_t>{}(p.second) << 1); }
        };
        std::unordered_map<std::pair<TypeId, uint64_t>, TypeId, PairHash> array_cache_;
    };

} // namespace edn

// ---- Phase 2 helper utilities for type/category queries (kept header-only for inlining) ----
namespace edn
{
    inline bool is_integer_base(BaseType b)
    {
        switch (b)
        {
        case BaseType::I1:
        case BaseType::I8:
        case BaseType::I16:
        case BaseType::I32:
        case BaseType::I64:
        case BaseType::U8:
        case BaseType::U16:
        case BaseType::U32:
        case BaseType::U64:
            return true;
        default:
            return false;
        }
    }
    inline bool is_signed_base(BaseType b)
    {
        switch (b)
        {
        case BaseType::I1:
        case BaseType::I8:
        case BaseType::I16:
        case BaseType::I32:
        case BaseType::I64:
            return true; // treat i1 as signed for extension purposes (LLVM allows either)
        default:
            return false;
        }
    }
    inline unsigned base_type_bit_width(BaseType b)
    {
        switch (b)
        {
        case BaseType::I1:
            return 1;
        case BaseType::I8:
        case BaseType::U8:
            return 8;
        case BaseType::I16:
        case BaseType::U16:
            return 16;
        case BaseType::I32:
        case BaseType::U32:
            return 32;
        case BaseType::I64:
        case BaseType::U64:
            return 64;
        case BaseType::F32:
            return 32;
        case BaseType::F64:
            return 64;
        case BaseType::Void:
            return 0;
        }
        return 0;
    }
    inline bool is_float_base(BaseType b) { return b == BaseType::F32 || b == BaseType::F64; }
}
