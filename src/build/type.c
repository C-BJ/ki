
#include "../all.h"

void type_allowed_async_error(Fc *fc, Type *type, Str *chain);

Type *type_init(Allocator *alc) {
    //
    Type *type = al(alc, sizeof(Type));
    type->type = type_void;
    type->bytes = 0;
    type->ptr_depth = 0;
    type->is_signed = false;
    type->nullable = false;
    type->class = NULL;
    type->enu = NULL;
    type->array_of = NULL;
    type->array_size = 0;
    //
    type->take_ownership = true;
    type->strict_ownership = false;
    //
    type->func_args = NULL;
    type->func_rett = NULL;
    type->func_errors = NULL;
    type->func_can_error = false;
    //
    return type;
}

int type_get_size(Build *b, Type *type) {
    //
    if (type->bytes > 0) {
        return type->bytes;
    }
    int size = 0;
    if (type->ptr_depth > 0) {
        size = b->ptr_size;
    } else if (type->class) {
        size = type->class->size;
    }
    type->bytes = size;
    return size;
}

bool type_is_void(Type *type) {
    //
    return type->type == type_void;
}
bool type_is_ptr(Type *type, Build *b) {
    //
    return type->class == ki_get_class(b, "type", "ptr");
}
bool type_is_bool(Type *type, Build *b) {
    //
    return type->class == ki_get_class(b, "type", "bool");
}
bool type_is_string(Type *type, Build *b) {
    //
    return type->class == ki_get_class(b, "type", "String");
}

Type *type_gen_class(Allocator *alc, Class *class) {
    //
    if (!class) {
        printf("Generating type for class that doesnt exist yet (compiler bug)");
        raise(11);
    }

    int type = type_ptr;
    int bytes = class->fc->b->ptr_size;
    bool is_float = false;
    bool depth = 1;
    bool sign = false;

    if (class->type == ct_struct) {
        type = type_struct;
    } else if (class->type == ct_ptr) {
        type = type_ptr;
    } else if (class->type == ct_int) {
        type = type_int;
        bytes = class->size;
        depth = 0;
        sign = class->is_signed;
    } else if (class->type == ct_float) {
        type = type_float;
        bytes = class->size;
        depth = 0;
        sign = class->is_signed;
    } else {
        die("Compiler bug, unknown class type");
    }

    Type *t = type_init(alc);
    t->type = type;
    t->bytes = bytes;
    t->ptr_depth = depth;
    t->is_signed = sign;
    t->class = class;
    return t;
}

Type *type_gen_fptr(Allocator *alc, Func *func) {
    //
    Type *t = type_init(alc);
    t->type = type_func_ptr;
    t->bytes = func->fc->b->ptr_size;
    t->ptr_depth = 1;

    t->func_args = func->args;
    t->func_rett = func->rett;
    t->func_errors = func->errors;
    t->func_can_error = func->can_error;

    return t;
}

Type *type_gen_void(Allocator *alc) {
    Type *t = type_init(alc);
    t->type = type_void;
    return t;
}

Type *read_type(Fc *fc, Allocator *alc, Scope *scope, bool sameline, bool allow_space, bool is_arg) {
    //
    char *token = fc->token;
    bool nullable = false;
    bool t_inline = false;
    bool async = false;

    bool take_ownership = is_arg ? false : true;
    bool strict_ownership = false;
    bool strict_ownership_overwrite = true;

    tok(fc, token, sameline, allow_space);

    if (strcmp(token, "async") == 0) {
        async = true;
        tok(fc, token, true, true);
    }

    // if (is_arg && strcmp(token, "&") == 0) {
    //     take_ownership = false;
    //     tok(fc, token, true, false);
    if (is_arg) {
        if (strcmp(token, ">") == 0) {
            take_ownership = true;
            tok(fc, token, true, false);
        }
    }

    if (strcmp(token, "?") == 0) {
        nullable = true;
        tok(fc, token, true, false);
    }

    if (strcmp(token, ".") == 0) {
        strict_ownership = true;
        tok(fc, token, true, false);
    }

    Type *type = NULL;
    if (strcmp(token, "void") == 0) {
        type = type_init(alc);
        type->type = type_void;
        return type;
    } else if (strcmp(token, "fn") == 0) {
        // Func Ref Type
        type = type_init(alc);
        type->type = type_func_ptr;
        type->bytes = fc->b->ptr_size;
        type->ptr_depth = 1;

        Array *args = array_make(alc, 2);

        tok_expect(fc, "(", true, true);
        tok(fc, token, true, true);
        while (strcmp(token, ")") != 0) {

            // bool take_ownership = false;
            // bool strict_ownership = false;
            // if (strcmp(token, "*") == 0) {
            //     take_ownership = true;
            //     tok(fc, token, true, true);
            // } else if (strcmp(token, "**") == 0) {
            //     take_ownership = true;
            //     strict_ownership = true;
            //     tok(fc, token, true, true);
            // } else {
            rtok(fc);
            // }

            Type *type = read_type(fc, alc, scope, true, true, true);
            // type->take_ownership = take_ownership;
            // type->strict_ownership = strict_ownership;

            Arg *arg = arg_init(alc, "", type, false);
            array_push(args, arg);
            tok(fc, token, true, true);
            if (strcmp(token, ",") == 0) {
                tok(fc, token, true, true);
            }
        }
        type->func_args = args;

        // Check return type
        tok_expect(fc, "(", true, false);
        type->func_rett = read_type(fc, alc, scope, true, true, false);
        tok_expect(fc, ")", true, true);

        // i = tok(fc, token, true, true);

        // if (strcmp(token, "or") == 0) {
        //     // Read error codes
        //     type->func_can_error = true;
        //     fc->i = i;

        //     type->func_error_codes = array_make(2);
        //     while (true) {
        //         fc->i = tok(fc, token, true, true);
        //         if (!is_valid_varname(token)) {
        //             fc_error(fc, "Invalid error code syntax: '%s'", token);
        //         }
        //         if (array_contains(type->func_error_codes, token, arr_find_str)) {
        //             fc_error(fc, "Duplicate error code: '%s'", token);
        //         }

        //         array_push(type->func_error_codes, strdup(token));

        //         i = tok(fc, token, true, true);
        //         if (strcmp(token, ",") == 0) {
        //             fc->i = i;
        //             continue;
        //         }
        //         break;
        //     }
        // }
    } else {

        rtok(fc);

        Id *id = read_id(fc, sameline, allow_space, false);
        Idf *idf = id ? idf_by_id(fc, scope, id, false) : NULL;

        if (!idf) {
            sprintf(fc->sbuf, "Unknown type: '%s'", token);
            fc_error(fc);
        } else {
            if (idf->type == idf_class) {
                Class *class = idf->item;
                // Check generic
                if (class->is_generic_base) {
                    //
                    Array *generic_types = read_generic_types(fc, scope, class);
                    class = class_get_generic_class(class, generic_types);
                }
                type = type_gen_class(alc, class);
                //
            } else if (idf->type == idf_type) {
                Type *t = idf->item;
                type = type_init(alc);
                *type = *t;
                strict_ownership_overwrite = false;
            } else if (idf->type == idf_enum) {
                type = type_gen(fc->b, alc, "i32");
                type->enu = idf->item;
            } else if (idf->type == idf_fc) {
                Fc *fc_ = idf->item;
                tok_expect(fc, ".", true, false);
                type = read_type(fc, alc, fc_->scope, true, false, false);
            } else {
                sprintf(fc->sbuf, "Not a type");
                fc_error(fc);
            }
        }
    }

    type->take_ownership = take_ownership;
    if (strict_ownership || strict_ownership_overwrite)
        type->strict_ownership = strict_ownership;

    if (async) {
        if (!type_allowed_async(type, true)) {
            Str *chain = str_make(alc, 500);
            type_allowed_async_error(fc, type, chain);
        }
    }

    tok(fc, token, true, false);
    while (strcmp(token, "[") == 0) {
        tok(fc, token, true, true);
        int count = -1;
        if (is_valid_number(token)) {
            count = atoi(token);
        } else {
            sprintf(fc->sbuf, "Invalid array size number (0-9 characters only)");
            fc_error(fc);
        }

        tok_expect(fc, "]", true, true);

        type = type_array_of(alc, fc->b, type, count);

        tok(fc, token, true, false);
    }
    rtok(fc);

    //
    if (nullable) {
        if (type->ptr_depth == 0) {
            sprintf(fc->sbuf, "Only pointer types can be nullable.");
            fc_error(fc);
        }
        type->nullable = nullable;
    }

    if (type->bytes == 0) {
        array_push(fc->type_size_checks, type);
    }

    return type;
}

Type *type_array_of(Allocator *alc, Build *b, Type *type, int size) {
    //
    Type *new = type_init(alc);
    new->type = type_arr;
    new->array_of = type;
    new->array_size = size;
    new->bytes = b->ptr_size;
    new->ptr_depth = 1;
    return new;
}

Type *type_clone(Allocator *alc, Type *type) {
    //
    Type *t = type_init(alc);
    *t = *type;
    return t;
}

Type *type_gen_int(Build *b, Allocator *alc, int bytes, bool is_signed) {
    if (bytes == 1) {
        if (is_signed) {
            return type_gen(b, alc, "i8");
        } else {
            return type_gen(b, alc, "u8");
        }
    } else if (bytes == 2) {
        if (is_signed) {
            return type_gen(b, alc, "i16");
        } else {
            return type_gen(b, alc, "u16");
        }
    } else if (bytes == 4) {
        if (is_signed) {
            return type_gen(b, alc, "i32");
        } else {
            return type_gen(b, alc, "u32");
        }
    } else if (bytes == 8) {
        if (is_signed) {
            return type_gen(b, alc, "i64");
        } else {
            return type_gen(b, alc, "u64");
        }
    }
    die("Cannot generate integer type (bug)");
}

Type *type_gen(Build *b, Allocator *alc, char *name) {

    Pkc *pkc = b->pkc_ki;
    Nsc *nsc = pkc_get_nsc(pkc, "type");

    Idf *idf = map_get(nsc->scope->identifiers, name);
    if (!idf || idf->type != idf_class) {
        sprintf(b->sbuf, "Type not found '%s'", name);
        die(b->sbuf);
    }

    return type_gen_class(alc, idf->item);
}

bool type_compat(Type *t1, Type *t2, char **reason) {
    //
    bool t1void = type_is_void(t1);
    bool t2void = type_is_void(t2);
    if (t1void || t2void) {
        if (reason)
            *reason = "One type is void, the other is not";
        return t1void == t2void;
    }
    int t1t = t1->type;
    int t2t = t2->type;
    if (t2t == type_null && t1->nullable) {
        return true;
    }
    if (t2->nullable && !t1->nullable) {
        if (reason)
            *reason = "Trying to assign nullable type to a non nullable type";
        return false;
    }
    if (t1t != t2t) {
        if (reason)
            *reason = "Different kind of types. E.g. number/pointer or integer/float";
        return false;
    }
    if (t1->bytes != t2->bytes) {
        if (reason)
            *reason = "The types have different sizes";
        return false;
    }
    if (t1->ptr_depth != t2->ptr_depth) {
        if (reason)
            *reason = "Pointer depth difference";
        return false;
    }
    bool t1o = type_tracks_ownership(t1);
    // bool t2o = type_tracks_ownership(t2);
    // if (t1o != t2o) {
    //     if (reason)
    //         *reason = "One type tracks ownership, the other does not";
    //     return false;
    // }
    if (t1o) {
        if (t1->take_ownership && !t2->take_ownership) {
            if (reason)
                *reason = "Trying to pass a type with borrowed ownership to a type that requires normal ownership";
            return false;
        }
        if (t1->strict_ownership && !t2->strict_ownership) {
            if (reason)
                *reason = "One type has strict ownership, the other does not";
            return false;
        }
    }
    if (t1t == type_int) {
        if (t1->enu != NULL && t1->enu != t2->enu) {
            if (reason)
                *reason = "Right side number must be from the same enum type";
            return false;
        }
        if (t2->is_signed != t1->is_signed) {
            if (reason)
                *reason = "Signed & unsigned are not compatible, use cast";
            return false;
        }
    } else if (t1t == type_struct) {
        if (t1->class != t2->class) {
            if (reason)
                *reason = "Classes are not the same";
            return false;
        }
    } else if (t1t == type_func_ptr) {
        Array *t1_args = t1->func_args;
        Array *t2_args = t2->func_args;
        if (t1_args->length != t2_args->length) {
            if (reason)
                *reason = "Number of arguments is not the same";
            return false;
        }
        for (int i = 0; i < t1_args->length; i++) {
            Arg *t1d = array_get_index(t1_args, i);
            Arg *t2d = array_get_index(t2_args, i);
            if (!type_compat(t1d->type, t2d->type, NULL)) {
                if (reason)
                    *reason = "Argument types are not compatible";
                return false;
            }
        }
        if (!type_compat(t1->func_rett, t2->func_rett, NULL)) {
            if (reason)
                *reason = "Function return types are not compatible";
            return false;
        }
        if (t1->func_can_error != t2->func_can_error) {
            if (reason)
                *reason = "One type can return errors, the other cannot";
            return false;
        }
    } else if (t1t == type_arr) {
        int t1as = t1->array_size;
        int t2as = t2->array_size;
        if (t2as != t1as && (t1as == -1 || (t2as != -1 && t2as < t1as))) {
            if (reason)
                *reason = "Right side array size is smaller than left side array size";
            return false;
        }
        if (!type_compat(t1->array_of, t2->array_of, NULL)) {
            if (reason)
                *reason = "Right side array item type is not compatible with left side array item type";
            return false;
        }
    }
    return true;
}

char *type_to_str(Type *t, char *res) {
    //
    int depth = t->ptr_depth;

    strcpy(res, "");
    // if (t->type == type_ptr && t->class == NULL) {
    //     strcat(res, "ptr");
    //     return res;
    // }
    if (type_tracks_ownership(t)) {
        if (!t->take_ownership) {
            strcat(res, "&");
        }
    }
    if (t->nullable) {
        strcat(res, "?");
    }
    if (t->strict_ownership) {
        strcat(res, ".");
    }

    if (t->class) {
        Class *class = t->class;

        if (class->type == ct_struct || class->type == ct_ptr)
            depth--;

        strcat(res, class->dname);
    } else if (t->type == type_null) {
        strcat(res, "null");
    } else if (t->type == type_void) {
        strcat(res, "void");
    } else if (t->type == type_func_ptr) {
        depth--;
        strcat(res, "fn(");
        char sub_str[256];
        for (int i = 0; i < t->func_args->length; i++) {
            if (i > 0) {
                strcat(res, ", ");
            }
            Arg *arg = array_get_index(t->func_args, i);
            strcat(res, type_to_str(arg->type, sub_str));
        }
        strcat(res, ")(");
        strcat(res, type_to_str(t->func_rett, sub_str));
        strcat(res, ")");
        if (t->func_errors) {
            strcat(res, "!");
        }
    } else if (t->type == type_arr) {
        char sub_str[256];
        strcat(res, type_to_str(t->array_of, sub_str));
        strcat(res, "[");
        char nr[10];
        sprintf(nr, "%d", t->array_size);
        strcat(res, t->array_size == -1 ? "" : nr);
        strcat(res, "]");
        depth--;
    } else {
        strcat(res, "(Unknown type)");
    }
    while (depth > 0) {
        depth--;
        strcat(res, "*");
    }
    return res;
}

void type_check(Fc *fc, Type *t1, Type *t2) {
    //
    char *reason;
    if (!type_compat(t1, t2, &reason)) {
        char t1s[200];
        char t2s[200];
        type_to_str(t1, t1s);
        type_to_str(t2, t2s);
        sprintf(fc->sbuf, "Types are not compatible: %s <-- %s \nReason: %s", t1s, t2s, reason);
        fc_error(fc);
    }
}

bool type_tracks_ownership(Type *type) {
    //
    if (type->strict_ownership) {
        return true;
    }
    Class *class = type->class;
    if (!class) {
        return false;
    }
    if (class->is_rc || class->must_ref || class->must_deref) {
        return true;
    }
    return false;
}

bool type_allowed_async(Type *type, bool recursive) {
    Class *class = type->class;
    if (!class || class->type == ct_int || class->type == ct_float) {
        return true;
    }
    if (!class->async && !type->strict_ownership) {
        return false;
    }
    if (class->type == ct_struct) {
        if (!recursive) {
            return true;
        }
        Array *props = class->props->values;
        int argc = props->length;
        for (int i = 0; i < argc; i++) {
            ClassProp *prop = array_get_index(props, i);
            if (!type_allowed_async(prop->type, recursive)) {
                return false;
            }
        }
        return true;
    }
    return true;
}

void type_allowed_async_error(Fc *fc, Type *type, Str *chain) {
    //
    bool first_type = chain->length == 0;

    char buf[256];
    type_to_str(type, buf);
    str_append_chars(chain, buf);

    if (!type_allowed_async(type, false)) {
        char *chain_str = str_to_chars(fc->alc, chain);
        sprintf(fc->sbuf, "Expected an async type. The '%s' struct/class does not support being used asynchronously. In order for a type to be used asynchronously, it must obey the following rule: The type must either have strict ownership or the class of the type must be marked with 'async'. This rule also applies to all of its properties.", chain_str);
        fc_error(fc);
    }

    Class *class = type->class;
    if (class && class->type == ct_struct) {
        Array *props = class->props->values;
        Array *names = class->props->keys;
        int argc = props->length;
        str_append_chars(chain, ".");
        int chain_reset = chain->length;
        for (int i = 0; i < argc; i++) {
            chain->length = chain_reset;
            ClassProp *prop = array_get_index(props, i);
            char *name = array_get_index(names, i);
            str_append_chars(chain, name);
            str_append_chars(chain, " -> ");
            type_allowed_async_error(fc, prop->type, chain);
        }
    }
}
