
#include "../../headers/LLVM.h"

void llvm_if(LB *b, Scope *scope, TIf *ift);

void llvm_write_ast(LB *b, Scope *scope) {
    //
    if (!scope->lvars)
        scope->lvars = map_make(b->alc);

    Allocator *alc = b->alc;

    Array *ast = scope->ast;
    for (int i = 0; i < ast->length; i++) {
        Token *t = array_get_index(ast, i);

        if (t->type == tkn_declare) {
            Decl *decl = t->item;
            Value *val = decl->value;

            char *lval = llvm_value(b, scope, val);

            if (!decl->is_mut) {
                decl->llvm_val = lval;
                map_set(scope->lvars, decl->name, lval);
                continue;
            }

            char *lvar = llvm_alloca(b, decl->type);
            decl->llvm_val = lvar;
            llvm_ir_store(b, decl->type, lvar, lval);
            map_set(scope->lvars, decl->name, lvar);
            continue;
        }
        if (t->type == tkn_assign) {
            VPair *pair = t->item;
            Value *left = pair->left;
            Type *lt = left->rett;
            char *lvar = llvm_assign_value(b, scope, left);
            char *lval = llvm_value(b, scope, pair->right);
            llvm_ir_store(b, lt, lvar, lval);
            continue;
        }
        if (t->type == tkn_ir_val) {
            IRVal *item = t->item;
            item->ir_value = llvm_value(b, scope, item->value);
            continue;
        }
        if (t->type == tkn_ir_assign_val) {
            IRAssignVal *item = t->item;
            item->ir_value = llvm_assign_value(b, scope, item->value);
            continue;
        }
        if (t->type == tkn_return) {
            Value *val = t->item;
            LLVMFunc *lfunc = b->lfunc;
            Str *ir = lfunc->block->ir;

            char *save_vn = lfunc->stack_save_vn;
            if (save_vn) {
                str_append_chars(ir, "  call void @llvm.stackrestore(i8* ");
                str_append_chars(ir, save_vn);
                str_append_chars(ir, ")\n");
            }
            char *lval = "void";
            if (val) {
                lval = llvm_value(b, scope, val);
                char *ltype = llvm_type(b, val->rett);
                char *buf = al(b->alc, strlen(lval) + 2 + strlen(ltype));
                strcpy(buf, ltype);
                strcat(buf, " ");
                strcat(buf, lval);
                lval = buf;
            }
            Str *rir = llvm_b_ir(b);
            str_append_chars(rir, "  ret ");
            str_append_chars(rir, lval);
            str_append_chars(rir, "\n");
            continue;
        }
        if (t->type == tkn_throw) {
            Throw *item = t->item;
            Type *err_code_type = type_gen(b->fc->b, b->alc, "i32");
            char code[20];
            sprintf(code, "%d", item->code);
            llvm_ir_store(b, err_code_type, "@ki_err_code_buffer", code);

            Type *rett = item->func->rett;
            char *ltype = llvm_type(b, rett);

            Str *ir = llvm_b_ir(b);
            str_append_chars(ir, "  ret ");
            str_append_chars(ir, ltype);
            if (!type_is_void(item->func->rett)) {
                str_append_chars(ir, " ");
                str_append_chars(ir, item->func->rett->ptr_depth > 0 ? "null" : "0");
            }
            str_append_chars(ir, "\n");
            continue;
        }
        if (t->type == tkn_if) {
            TIf *ift = t->item;
            llvm_if(b, scope, ift);
            continue;
        }
        if (t->type == tkn_while) {
            TWhile *w = t->item;

            Value *cond = w->cond;
            Scope *sub = w->scope;

            LLVMBlock *b_cond = llvm_block_init_auto(b);
            LLVMBlock *b_code = llvm_block_init_auto(b);
            LLVMBlock *b_after = llvm_block_init_auto(b);
            LLVMBlock *prev_cond = b->while_cond;
            LLVMBlock *prev_after = b->while_after;
            b->while_cond = b_cond;
            b->while_after = b_after;

            llvm_ir_jump(llvm_b_ir(b), b_cond);

            b->lfunc->block = b_cond;
            char *lcond = llvm_value(b, scope, cond);
            Str *ir_c = llvm_b_ir(b);
            char *lcond_i1 = llvm_ir_bool_i1(b, ir_c, lcond);
            llvm_ir_cond_jump(b, ir_c, lcond_i1, b_code, b_after);

            b->lfunc->block = b_code;
            llvm_write_ast(b, sub);
            if (!sub->did_return) {
                llvm_ir_jump(llvm_b_ir(b), b_cond);
            }

            b->lfunc->block = b_after;
            b->while_cond = prev_cond;
            b->while_after = prev_after;
            continue;
        }
        if (t->type == tkn_break) {
            LLVMBlock *after_block = b->while_after;
            llvm_ir_jump(llvm_b_ir(b), after_block);
            continue;
        }
        if (t->type == tkn_continue) {
            LLVMBlock *cond_block = b->while_cond;
            llvm_ir_jump(llvm_b_ir(b), cond_block);
            continue;
        }
        if (t->type == tkn_statement) {
            Value *val = t->item;
            char *lval = llvm_value(b, scope, val);
            continue;
        }
        if (t->type == tkn_exec) {
            TExec *exec = t->item;
            if (exec->enable)
                llvm_write_ast(b, exec->scope);
            continue;
        }
    }
}

void llvm_if(LB *b, Scope *scope, TIf *ift) {
    //
    Value *cond = ift->cond;
    Scope *sub = ift->scope;
    Scope *else_scope = ift->else_scope;
    Scope *deref_scope = ift->deref_scope;

    LLVMBlock *code_block = llvm_block_init_auto(b);
    LLVMBlock *else_block = llvm_block_init_auto(b);
    LLVMBlock *after = llvm_block_init_auto(b);

    char *lcond = llvm_value(b, scope, cond);

    if (deref_scope)
        llvm_write_ast(b, deref_scope);

    Str *ir = llvm_b_ir(b);
    char *lcond_i1 = llvm_ir_bool_i1(b, ir, lcond);
    llvm_ir_cond_jump(b, ir, lcond_i1, code_block, else_block);

    b->lfunc->block = code_block;
    llvm_write_ast(b, sub);
    if (!sub->did_return) {
        Str *ir = llvm_b_ir(b);
        llvm_ir_jump(ir, after);
    }

    b->lfunc->block = else_block;
    if (else_scope)
        llvm_write_ast(b, else_scope);
    if (!else_scope || !else_scope->did_return) {
        Str *ir = llvm_b_ir(b);
        llvm_ir_jump(ir, after);
    }

    b->lfunc->block = after;
}
