
#include "../all.h"

void stage_6_func(Fc *fc, Func *func);

void token_declare(Allocator *alc, Fc *fc, Scope *scope);
void token_return(Allocator *alc, Fc *fc, Scope *scope);
TIf *token_if(Allocator *alc, Fc *fc, Scope *scope, bool has_cond);

void stage_6(Fc *fc) {
    //
    if (fc->is_header)
        return;

    Build *b = fc->b;
    if (b->verbose > 1) {
        printf("# Stage 6 : AST : %s\n", fc->path_ki);
    }

    for (int i = 0; i < fc->funcs->length; i++) {
        Func *func = array_get_index(fc->funcs, i);
        if (b->verbose > 1) {
            printf("> Read func AST: %s\n", func->dname);
        }
        stage_6_func(fc, func);
    }

    // Write IR
}

void stage_6_func(Fc *fc, Func *func) {
    //

    if (func->is_generated) {
        return;
    }

    Chunk *chunk = func->chunk_body;
    fc->chunk = chunk;

    read_ast(fc, func->scope, false);
}

void read_ast(Fc *fc, Scope *scope, bool single_line) {
    //
    char *token = fc->token;
    bool first_line = true;
    Allocator *alc = fc->alc_ast;

    while (true) {
        //
        tok(fc, token, false, true);

        if (token[0] == 0) {
            sprintf(fc->sbuf, "Unexpected end of file");
            fc_error(fc);
        }

        if (strcmp(token, "#") == 0) {
            read_macro(fc, alc, scope);
            continue;
        }
        first_line = false;

        if (strcmp(token, "}") == 0 && !single_line) {
            break;
        }

        if (strcmp(token, "let") == 0) {
            token_declare(alc, fc, scope);
            continue;
        }

        if (strcmp(token, "return") == 0) {
            token_return(alc, fc, scope);
            continue;
        }

        if (strcmp(token, "if") == 0) {
            TIf *tif = token_if(alc, fc, scope, true);
            array_push(scope->ast, token_init(alc, tkn_if, tif));
            continue;
        }

        //
        rtok(fc);
        Value *val = read_value(fc, alc, scope, false, 0);

        // Assign
        if (value_assignable(val)) {
            tok(fc, token, false, true);
            sprintf(fc->sbuf, ".%s.", token);
            if (strstr(".=.+=.-=.*=./=.", fc->sbuf)) {
                char *sign = dups(alc, token);
                if (val->type == v_var) {
                    Var *var = val->item;
                    if (!var->is_mut) {
                        sprintf(fc->sbuf, "Cannot assign value to an immutable variable");
                        fc_error(fc);
                    }
                    if (var->is_arg && val->rett->class->is_rc) {
                        sprintf(fc->sbuf, "Assigning new values to argument variables is not allowed for performance reasons");
                        fc_error(fc);
                    }
                }
                if (val->type == v_class_pa) {
                    VClassPA *pa = val->item;
                    ClassProp *prop = pa->prop;
                    // if prop.act != AccessType.public {
                    // 	if !scope.is_sub_scope_of(prop.class.scope) {
                    // 		fc.error("Trying to assign to a non-public property outside the class");
                    // 	}
                    // }
                }

                Value *right = read_value(fc, alc, scope, false, 0);
                if (type_is_void(right->rett)) {
                    sprintf(fc->sbuf, "Assignment invalid, right side does not return a value");
                    fc_error(fc);
                }

                if (strcmp(sign, "=") == 0) {
                } else if (strcmp(sign, "+=") == 0) {
                    right = value_op(fc, alc, scope, val, right, op_add);
                } else if (strcmp(sign, "-=") == 0) {
                    right = value_op(fc, alc, scope, val, right, op_sub);
                } else if (strcmp(sign, "*=") == 0) {
                    right = value_op(fc, alc, scope, val, right, op_mul);
                } else if (strcmp(sign, "/=") == 0) {
                    right = value_op(fc, alc, scope, val, right, op_div);
                }

                right = try_convert(fc, alc, right, val->rett);
                type_check(fc, val->rett, right->rett);

                array_push(scope->ast, tgen_assign(alc, val, right));
                tok_expect(fc, ";", false, true);
                continue;
            }
            rtok(fc);
        }

        // Statement
        if (type_is_void(val->rett)) {
            sprintf(fc->sbuf, "Statement returns a value, but no variable to store it in");
            fc_error(fc);
        }

        array_push(scope->ast, token_init(alc, tkn_statement, val));
        tok_expect(fc, ";", false, true);
    }
}

void token_declare(Allocator *alc, Fc *fc, Scope *scope) {
    //
    char *token = fc->token;

    tok(fc, token, true, true);

    bool mutable = false;

    if (strcmp(token, "mut") == 0) {
        mutable = true;
        tok(fc, token, true, true);
    }
    if (is_valid_varname_char(token[0])) {
        sprintf(fc->sbuf, "Invalid variable name syntax '%s'", token);
    }

    char *name = dups(alc, token);

    if (map_get(scope->identifiers, name)) {
        sprintf(fc->sbuf, "Variable name already used: '%s'", name);
    }

    Type *type = NULL;

    tok(fc, token, false, true);
    if (strcmp(token, ":") == 0) {
        type = read_type(fc, alc, scope, false, true);
        tok(fc, token, false, true);
    }

    if (strcmp(token, "=") != 0) {
        sprintf(fc->sbuf, "Expected '='");
        fc_error(fc);
    }

    Value *val = read_value(fc, alc, scope, false, 0);

    if (type) {
        val = try_convert(fc, alc, val, type);
        type_check(fc, type, val->rett);
    } else {
        type = val->rett;
    }

    tok_expect(fc, ";", false, true);

    Var *var = var_init(alc, name, type, mutable, false, false);
    array_push(scope->ast, tgen_declare(alc, var, val));

    Idf *idf = idf_init(alc, idf_var);
    idf->item = var;

    map_set(scope->identifiers, name, idf);
}

void token_return(Allocator *alc, Fc *fc, Scope *scope) {
    //
    Scope *fscope = scope_find(scope, sct_func);
    if (!fscope) {
        sprintf(fc->sbuf, "Using 'return' outside function scope");
        fc_error(fc);
    }

    Func *func = fscope->func;
    Type *frett = func->rett;
    Value *retval = NULL;

    if (!type_is_void(frett)) {
        Value *val = read_value(fc, alc, scope, true, 0);
        Value *tval = try_convert(fc, alc, val, frett);
        type_check(fc, frett, tval->rett);
        retval = tval;
    }

    array_push(scope->ast, tgen_return(alc, fscope, retval));
    tok_expect(fc, ";", false, true);
}

TIf *token_if(Allocator *alc, Fc *fc, Scope *scope, bool has_cond) {
    //
    char *token = fc->token;
    Value *cond = NULL;

    if (has_cond) {
        Value *val = read_value(fc, alc, scope, true, 0);
        if (!type_is_bool(val->rett, fc->b)) {
            sprintf(fc->sbuf, "Condition value must return a bool");
            fc_error(fc);
        }
        cond = val;
    }

    tok(fc, token, false, true);
    bool single = strcmp(token, "{") != 0;
    if (single) {
        rtok(fc);
    }

    Scope *sub = scope_init(alc, sct_default, scope, true);

    read_ast(fc, sub, single);

    tok(fc, token, false, true);
    TIf *else_if = NULL;
    if (strcmp(token, "else") == 0) {
        tok(fc, token, true, true);
        bool has_if = strcmp(token, "if") == 0;
        if (!has_if) {
            rtok(fc);
        }
        else_if = token_if(alc, fc, scope, has_if);
    } else {
        rtok(fc);
    }

    return tgen_tif(alc, cond, sub, else_if);
}
