
#include "../headers/LLVM.h"

void stage_7(Fc *fc) {
    //
    Build *b = fc->b;
    if (b->verbose > 1) {
        printf("# Stage 7 : LLVM IR : %s\n", fc->path_ki);
    }

    Allocator *alc = fc->alc_ast;

    LB *lb = al(alc, sizeof(LB));
    lb->fc;
    lb->alc = alc;

    lb->_lfunc = NULL;
    lb->lfuncs = array_make(alc, fc->funcs->length + 1);
    lb->defined_classes = array_make(alc, fc->classes->length + 1);
    lb->defined_funcs = array_make(alc, fc->funcs->length + 1);

    lb->ir_final = str_make(alc, 2000);
    lb->ir_struct = str_make(alc, 1000);
    lb->ir_global = str_make(alc, 1000);
    lb->ir_extern_func = str_make(alc, 500);

    lb->strc = 0;
    lb->while_cond = NULL;
    lb->while_after = NULL;

    llvm_build_ir(lb);

    char *ir = str_to_chars(alc, lb->ir_final);

    char ir_hash[64];
    md5(ir, ir_hash);

    if (strcmp(fc->ir_hash, ir_hash) != 0) {

        fc->ir_hash = ir_hash;
        fc->ir_changed = true;

        write_file(fc->path_ir, ir, false);
    }
}

//
