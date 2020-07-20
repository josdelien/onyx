#include "onyxtypes.h"
#include "onyxastnodes.h"
#include "onyxutils.h"

// NOTE: These have to be in the same order as Basic
Type basic_types[] = {
    { Type_Kind_Basic, 0, { Basic_Kind_Void,                    0,                       0, "void"   } },

    { Type_Kind_Basic, 0, { Basic_Kind_Bool,   Basic_Flag_Boolean,                       1, "bool"   } },

    { Type_Kind_Basic, 0, { Basic_Kind_I8,     Basic_Flag_Integer,                       1, "i8"     } },
    { Type_Kind_Basic, 0, { Basic_Kind_U8,     Basic_Flag_Integer | Basic_Flag_Unsigned, 1, "u8"     } },
    { Type_Kind_Basic, 0, { Basic_Kind_I16,    Basic_Flag_Integer,                       2, "i16"    } },
    { Type_Kind_Basic, 0, { Basic_Kind_U16,    Basic_Flag_Integer | Basic_Flag_Unsigned, 2, "u16"    } },
    { Type_Kind_Basic, 0, { Basic_Kind_I32,    Basic_Flag_Integer,                       4, "i32"    } },
    { Type_Kind_Basic, 0, { Basic_Kind_U32,    Basic_Flag_Integer | Basic_Flag_Unsigned, 4, "u32"    } },
    { Type_Kind_Basic, 0, { Basic_Kind_I64,    Basic_Flag_Integer,                       8, "i64"    } },
    { Type_Kind_Basic, 0, { Basic_Kind_U64,    Basic_Flag_Integer | Basic_Flag_Unsigned, 8, "u64"    } },

    { Type_Kind_Basic, 0, { Basic_Kind_F32,    Basic_Flag_Float,                         4, "f32"    } },
    { Type_Kind_Basic, 0, { Basic_Kind_F64,    Basic_Flag_Float,                         8, "f64"    } },

    { Type_Kind_Basic, 0, { Basic_Kind_Rawptr, Basic_Flag_Pointer,                       4, "rawptr" } },
};

b32 types_are_surface_compatible(Type* t1, Type* t2) {
    // NOTE: If they are pointing to the same thing,
    // it is safe to assume they are the same type
    if (t1 == t2) return 1;
    if (t1 == NULL || t2 == NULL) return 0;

    switch (t1->kind) {
        case Type_Kind_Basic:
            if (t2->kind == Type_Kind_Basic) {
                // HACK: Not sure if this is right way to check this?
                if (t1 == t2) return 1;

                if ((t1->Basic.flags & Basic_Flag_Integer) && (t2->Basic.flags & Basic_Flag_Integer)) {
                    return t1->Basic.size == t2->Basic.size;
                }
            }
            break;

        case Type_Kind_Pointer:
            if (t2->kind == Type_Kind_Pointer) return 1;
            break;

        case Type_Kind_Struct: {
            if (t2->kind != Type_Kind_Struct) return 0;
            if (t1->Struct.mem_count != t2->Struct.mem_count) return 0;
            if (strcmp(t1->Struct.name, t2->Struct.name) == 0) return 1;

            b32 works = 1;
            bh_table_each_start(StructMember, t1->Struct.members);
                if (!bh_table_has(StructMember, t2->Struct.members, (char *) key)) return 0;
                StructMember other = bh_table_get(StructMember, t2->Struct.members, (char *) key);
                if (other.offset != value.offset) return 0;

                if (!types_are_compatible(value.type, other.type)) {
                    works = 0;
                    break;
                }
            bh_table_each_end;

            return works;
        }

        default:
            assert(("Invalid type", 0));
            break;
    }

    return 0;
}

b32 types_are_compatible(Type* t1, Type* t2) {
    // NOTE: If they are pointing to the same thing,
    // it is safe to assume they are the same type
    if (t1 == t2) return 1;
    if (t1 == NULL || t2 == NULL) return 0;

    switch (t1->kind) {
        case Type_Kind_Basic:
            if (t2->kind == Type_Kind_Basic) {
                // HACK: Not sure if this is right way to check this?
                if (t1 == t2) return 1;

                if ((t1->Basic.flags & Basic_Flag_Integer) && (t2->Basic.flags & Basic_Flag_Integer)) {
                    return t1->Basic.size == t2->Basic.size;
                }
            }
            break;

        case Type_Kind_Pointer:
            if (t2->kind == Type_Kind_Pointer) {
                return types_are_compatible(t1->Pointer.elem, t2->Pointer.elem);
            }
            break;

        case Type_Kind_Struct: {
            if (t2->kind != Type_Kind_Struct) return 0;
            if (t1->Struct.mem_count != t2->Struct.mem_count) return 0;
            if (strcmp(t1->Struct.name, t2->Struct.name) == 0) return 1;

            b32 works = 1;
            bh_table_each_start(StructMember, t1->Struct.members);
                if (!bh_table_has(StructMember, t2->Struct.members, (char *) key)) return 0;
                StructMember other = bh_table_get(StructMember, t2->Struct.members, (char *) key);
                if (other.offset != value.offset) return 0;

                if (!types_are_surface_compatible(value.type, other.type)) {
                    works = 0;
                    break;
                }
            bh_table_each_end;

            return works;
        }

        default:
            assert(("Invalid type", 0));
            break;
    }

    return 0;
}

u32 type_size_of(Type* type) {
    if (type == NULL) return 0;

    switch (type->kind) {
        case Type_Kind_Basic:    return type->Basic.size;
        case Type_Kind_Pointer:  return 4;
        case Type_Kind_Function: return 0;
        case Type_Kind_Struct:   return type->Struct.size;
        default:                 return 0;
    }
}

Type* type_build_from_ast(bh_allocator alloc, AstType* type_node) {
    if (type_node == NULL) return NULL;

    switch (type_node->kind) {
        case Ast_Kind_Pointer_Type: {
            return type_make_pointer(alloc, type_build_from_ast(alloc, ((AstPointerType *) type_node)->elem));
        }

        case Ast_Kind_Function_Type: {
            AstFunctionType* ftype_node = (AstFunctionType *) type_node;
            u64 param_count = ftype_node->param_count;

            Type* func_type = bh_alloc(alloc, sizeof(Type) + sizeof(Type *) * param_count);

            func_type->kind = Type_Kind_Function;
            func_type->Function.param_count = param_count;
            func_type->Function.return_type = type_build_from_ast(alloc, ftype_node->return_type);

            if (param_count > 0)
                fori (i, 0, param_count - 1) {
                    func_type->Function.params[i] = type_build_from_ast(alloc, ftype_node->params[i]);
                }

            return func_type;
        }

        case Ast_Kind_Struct_Type: {
            AstStructType* s_node = (AstStructType *) type_node;
            if (s_node->stcache != NULL) return s_node->stcache;

            Type* s_type = bh_alloc(alloc, sizeof(Type));
            s_node->stcache = s_type;
            s_type->kind = Type_Kind_Struct;

            s_type->Struct.name = s_node->name;
            s_type->Struct.mem_count = bh_arr_length(s_node->members);
            bh_table_init(global_heap_allocator, s_type->Struct.members, s_type->Struct.mem_count);

            u32 offset = 0;
            u32 min_alignment = 1;
            bh_arr_each(AstStructMember *, member, s_node->members) {
                (*member)->type = type_build_from_ast(alloc, (*member)->type_node);

                // TODO: Add alignment checking here

                StructMember smem = {
                    .offset = offset,
                    .type = (*member)->type
                };

                token_toggle_end((*member)->token);
                bh_table_put(StructMember, s_type->Struct.members, (*member)->token->text, smem);
                token_toggle_end((*member)->token);

                offset += type_size_of((*member)->type);
            }

            // TODO: Again, add alignment
            s_type->Struct.size = offset;

            return s_type;
        }

        case Ast_Kind_Basic_Type:
            return ((AstBasicType *) type_node)->type;

        case Ast_Kind_Symbol:
            assert(("symbol node in type expression", 0));
            return NULL;

        default:
            assert(("Node is not a type node", 0));
            return NULL;
    }
}

Type* type_make_pointer(bh_allocator alloc, Type* to) {
    Type* ptr_type = bh_alloc_item(alloc, Type);

    ptr_type->kind = Type_Kind_Pointer;
    ptr_type->Pointer.base.flags |= Basic_Flag_Pointer;
    ptr_type->Pointer.base.size = 4;
    ptr_type->Pointer.elem = to;

    return ptr_type;
}

const char* type_get_name(Type* type) {
    if (type == NULL) return "unknown";

    switch (type->kind) {
        case Type_Kind_Basic: return type->Basic.name;
        case Type_Kind_Pointer: return bh_aprintf(global_scratch_allocator, "^%s", type_get_name(type->Pointer.elem));
        case Type_Kind_Struct: return type->Struct.name;
        default: return "unknown";
    }
}

u32 type_get_alignment_log2(Type* type) {
    i32 store_size = type_size_of(type);
    i32 is_integer = (type->Basic.flags & Basic_Flag_Integer)
                  || (type->Basic.flags & Basic_Flag_Pointer);

    if (is_integer) {
        if      (store_size == 1) return 0;
        else if (store_size == 2) return 1;
        else if (store_size == 4) return 2;
        else if (store_size == 8) return 3;
    } else {
        if      (store_size == 4) return 2;
        else if (store_size == 8) return 3;
    }

    return 2;
}

StructMember type_struct_lookup_member(Type* type, char* member) {
    if (!type_is_struct(type)) return (StructMember) { 0 };
    if (type->kind == Type_Kind_Pointer) type = type->Pointer.elem;

    TypeStruct* stype = &type->Struct;

    if (!bh_table_has(StructMember, stype->members, member)) return (StructMember) { 0 };
    StructMember smem = bh_table_get(StructMember, stype->members, member);
    return smem;
}

b32 type_is_pointer(Type* type) {
    return type->kind == Type_Kind_Pointer || (type->Basic.flags & Basic_Flag_Pointer) != 0;
}

b32 type_is_struct(Type* type) {
    if (type->kind == Type_Kind_Struct) return 1;
    if (type->kind == Type_Kind_Pointer && type->Pointer.elem->kind == Type_Kind_Struct) return 1;
    return 0;
}

b32 type_is_bool(Type* type) {
    return type != NULL && type->kind == Type_Kind_Basic && type->Basic.kind == Basic_Kind_Bool;
}