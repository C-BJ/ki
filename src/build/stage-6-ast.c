
#include "../all.h"

void stage_6_func(Fc *fc, Func *func);

void token_declare(Allocator *alc, Fc *fc, Scope *scope, bool replace);
void token_return(Allocator *alc, Fc *fc, Scope *scope);
void token_if(Allocator *alc, Fc *fc, Scope *scope);
void token_while(Allocator *alc, Fc *fc, Scope *scope);

void stage_6(Fc *fc) {
    //
    if (fc->is_header)
        return;

    Build *b = fc->b;
    if (b->verbose > 2) {
        printf("# Stage 6 : AST : %s\n", fc->path_ki);
    }

    for (int i = 0; i < fc->funcs->length; i++) {
        Func *func = array_get_index(fc->funcs, i);
        if (!func->chunk_body)
            continue;
        if (b->verbose > 2) {
            printf("> Read func AST: %s\n", func->dname);
        }
        stage_6_func(fc, func);
    }

    // Write IR
    stage_7(fc);
}

void stage_6_func(Fc *fc, Func *func) {
    //

    if (func->is_generated) {
        return;
    }

    Chunk *chunk = func->chunk_body;
    fc->chunk = chunk;

    read_ast(fc, func->scope, false);

    if (!type_is_void(func->rett) && !func->scope->did_return) {
        sprintf(fc->sbuf, "Function did not return a value");
        fc_error(fc);
    }
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

        if (scope->did_return) {
            if (single_line) {
                rtok(fc);
                break;
            }
            if (strcmp(token, "}") != 0) {
                sprintf(fc->sbuf, "Expected '}'");
                fc_error(fc);
            }
            break;
        }

        if (single_line && !first_line) {
            rtok(fc);
            break;
        }
        first_line = false;

        if (strcmp(token, "}") == 0 && !single_line) {
            break;
        }

        if (strcmp(token, "let") == 0) {
            token_declare(alc, fc, scope, false);
            continue;
        }
        if (strcmp(token, "rep") == 0) {
            token_declare(alc, fc, scope, true);
            continue;
        }

        if (strcmp(token, "return") == 0) {
            token_return(alc, fc, scope);
            continue;
        }
        if (strcmp(token, "break") == 0) {
            Scope *loop = scope_find(scope, sct_loop);
            if (!loop) {
                sprintf(fc->sbuf, "You can only use 'break' inside a loop");
                fc_error(fc);
            }
            deref_scope(alc, scope, loop);
            array_push(scope->ast, token_init(alc, tkn_break, loop));
            tok_expect(fc, ";", false, true);
            scope->did_return = true;
            continue;
        }
        if (strcmp(token, "continue") == 0) {
            Scope *loop = scope_find(scope, sct_loop);
            if (!loop) {
                sprintf(fc->sbuf, "You can only use 'break' inside a loop");
                fc_error(fc);
            }
            deref_scope(alc, scope, loop);
            array_push(scope->ast, token_init(alc, tkn_continue, loop));
            tok_expect(fc, ";", false, true);
            scope->did_return = true;
            continue;
        }

        if (strcmp(token, "if") == 0) {
            token_if(alc, fc, scope);
            continue;
        }

        if (strcmp(token, "while") == 0) {
            token_while(alc, fc, scope);
            continue;
        }

        rtok(fc);

        // printf("%s | %d\n", fc->path_ki, fc->chunk->line);
        Value *left = read_value(fc, alc, scope, false, 0, true);

        // Assign
        if (value_is_assignable(left)) {
            tok(fc, token, false, true);
            sprintf(fc->sbuf, ".%s.", token);
            if (strstr(".=.+=.-=.*=./=.", fc->sbuf)) {
                char *sign = dups(alc, token);

                if (left->type == v_var) {
                    Var *var = left->item;
                    Decl *decl = var->decl;
                    if (!decl->is_mut) {
                        sprintf(fc->sbuf, "Cannot assign value to an immutable variable");
                        fc_error(fc);
                    }
                }
                if (left->type == v_class_pa) {
                    VClassPA *pa = left->item;
                    ClassProp *prop = pa->prop;
                    // if prop.act != AccessType.public {
                    // 	if !scope.is_sub_scope_of(prop.class.scope) {
                    // 		fc.error("Trying to assign to a non-public property outside the class");
                    // 	}
                    // }
                }

                Value *right = read_value(fc, alc, scope, false, 0, false);
                if (type_is_void(right->rett)) {
                    sprintf(fc->sbuf, "Assignment invalid, right side does not return a value");
                    fc_error(fc);
                }

                if (strcmp(sign, "=") == 0) {
                } else if (strcmp(sign, "+=") == 0) {
                    right = value_op(fc, alc, scope, left, right, op_add);
                } else if (strcmp(sign, "-=") == 0) {
                    right = value_op(fc, alc, scope, left, right, op_sub);
                } else if (strcmp(sign, "*=") == 0) {
                    right = value_op(fc, alc, scope, left, right, op_mul);
                } else if (strcmp(sign, "/=") == 0) {
                    right = value_op(fc, alc, scope, left, right, op_div);
                }

                right = try_convert(fc, alc, right, left->rett);
                type_check(fc, left->rett, right->rett);

                right = usage_move_value(alc, fc->chunk, scope, right);

                Value *ir_right = vgen_ir_val(alc, right, right->rett);
                array_push(scope->ast, token_init(alc, tkn_ir_val, ir_right->item));

                if (left->type == v_var) {
                    Var *var = left->item;
                    Decl *decl = var->decl;
                    UsageLine *ul = usage_line_get(scope, decl);
                    end_usage_line(alc, ul);
                }

                array_push(scope->ast, tgen_assign(alc, left, ir_right));
                tok_expect(fc, ";", false, true);

                if (left->type == v_var) {
                    Var *var = left->item;
                    Decl *decl = var->decl;
                    usage_line_init(alc, scope, decl);
                }

                continue;
            }
            rtok(fc);
        }

        // Statement
        Value *val = left;
        Type *rett = val->rett;
        Class *class = rett->class;

        if (!type_is_void(val->rett) && (class->must_deref || class->must_ref)) {
            sprintf(fc->sbuf, "Statement returns a value, but no variable to store it in");
            fc_error(fc);
        }

        array_push(scope->ast, token_init(alc, tkn_statement, val));
        tok_expect(fc, ";", false, true);
    }

    // Derefs
    if (!scope->did_return) {
        deref_expired_decls(alc, scope);
    }
}

void token_declare(Allocator *alc, Fc *fc, Scope *scope, bool replace) {
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
    if (replace) {
        Idf *idf = map_get(scope->identifiers, token);
        if (!idf) {
            sprintf(fc->sbuf, "Variable not found, nothing to replace '%s'", token);
            fc_error(fc);
        }
        map_unset(scope->upref_slots, token);
    } else {
        name_taken_check(fc, scope, token);
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

    Value *val = read_value(fc, alc, scope, false, 0, false);

    if (type) {
        val = try_convert(fc, alc, val, type);
        type_check(fc, type, val->rett);
    } else {
        type = val->rett;
    }

    val = usage_move_value(alc, fc->chunk, scope, val);

    if (type_is_void(type)) {
        sprintf(fc->sbuf, "Variable declaration: Right side does not return a value");
        fc_error(fc);
    }

    tok_expect(fc, ";", false, true);

    Decl *decl = decl_init(alc, scope, name, type, val, mutable, false, false);
    array_push(scope->ast, token_init(alc, tkn_declare, decl));

    Var *var = var_init(alc, decl, type);

    Idf *idf = idf_init(alc, idf_var);
    idf->item = var;

    map_set(scope->identifiers, name, idf);

    usage_line_init(alc, scope, decl);
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
        Value *val = read_value(fc, alc, scope, true, 0, false);
        val = try_convert(fc, alc, val, frett);
        type_check(fc, frett, val->rett);

        val = usage_move_value(alc, fc->chunk, scope, val);

        IRVal *tvar = al(alc, sizeof(IRVal));
        tvar->value = val;
        tvar->ir_value = NULL;
        Token *t = token_init(alc, tkn_ir_val, tvar);
        array_push(scope->ast, t);

        Value *tmp_var = value_init(alc, v_ir_val, tvar, val->rett);
        retval = tmp_var;
    }

    deref_scope(alc, scope, fscope);

    array_push(scope->ast, tgen_return(alc, fscope, retval));
    tok_expect(fc, ";", false, true);

    scope->did_return = true;
}

void token_if(Allocator *alc, Fc *fc, Scope *scope) {
    //
    char *token = fc->token;

    Value *cond = read_value(fc, alc, scope, true, 0, false);
    if (!type_is_bool(cond->rett, fc->b)) {
        sprintf(fc->sbuf, "Condition value must return a bool");
        fc_error(fc);
    }

    tok(fc, token, false, true);
    bool single = strcmp(token, "{") != 0;
    if (single) {
        rtok(fc);
    }

    Scope *sub = usage_scope_init(alc, scope, sct_default);
    Scope *else_scope = usage_scope_init(alc, scope, sct_default);

    read_ast(fc, sub, single);

    tok(fc, token, false, true);
    if (strcmp(token, "else") == 0) {
        tok(fc, token, true, true);
        bool has_if = strcmp(token, "if") == 0;
        if (has_if) {
            token_if(alc, fc, else_scope);
        } else {
            rtok(fc);
            tok(fc, token, false, true);
            bool single = strcmp(token, "{") != 0;
            if (single) {
                rtok(fc);
            }
            read_ast(fc, else_scope, single);
        }
    } else {
        rtok(fc);
    }

    Scope *deref_scope = usage_create_deref_scope(alc, scope);

    Array *ancestors = array_make(alc, 2);
    array_push(ancestors, sub);
    array_push(ancestors, else_scope);
    usage_merge_ancestors(alc, scope, ancestors);

    TIf *tif = tgen_tif(alc, cond, sub, else_scope, deref_scope);
    array_push(scope->ast, token_init(alc, tkn_if, tif));
}

void token_while(Allocator *alc, Fc *fc, Scope *scope) {
    //
    Scope *sub = usage_scope_init(alc, scope, sct_loop);
    // Scope *sub = scope_init(alc, sct_loop, scope, true);

    Value *cond = read_value(fc, alc, sub, true, 0, false);

    if (!type_is_bool(cond->rett, fc->b)) {
        sprintf(fc->sbuf, "Value must return a bool type");
        fc_error(fc);
    }

    tok_expect(fc, "{", false, true);

    read_ast(fc, sub, false);

    // usage_clear_ancestors(scope);
    Array *ancestors = array_make(alc, 2);
    array_push(ancestors, sub);
    usage_merge_ancestors(alc, scope, ancestors);

    array_push(scope->ast, tgen_while(alc, cond, sub));
}
