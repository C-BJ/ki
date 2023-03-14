
#include "../all.h"

Token *token_init(Allocator *alc, int type, void *item) {
    //
    Token *t = al(alc, sizeof(Token));
    t->type = type;
    t->item = item;

    return t;
}

TIf *tgen_tif(Allocator *alc, Value *cond, Scope *scope, TIf *else_if) {
    //
    TIf *tif = al(alc, sizeof(TIf));
    tif->cond = cond;
    tif->scope = scope;
    tif->else_if = else_if;
    return tif;
}

Token *tgen_assign(Allocator *alc, Value *left, Value *right) {
    //
    VPair *pair = al(alc, sizeof(VPair));
    pair->left = left;
    pair->right = right;
    return token_init(alc, tkn_assign, pair);
}

Token *tgen_return(Allocator *alc, Scope *fscope, Value *retv) {
    //
    fscope->did_return = true;
    return token_init(alc, tkn_return, retv);
}

Token *tgen_while(Allocator *alc, Value *cond, Scope *scope) {
    //
    TWhile *w = al(alc, sizeof(TWhile));
    w->cond = cond;
    w->scope = scope;
    return token_init(alc, tkn_while, w);
}

Token *tgen_deref_decl_used(Allocator *alc, Decl *decl, Scope *scope) {
    //
    TDerefDeclUsed *item = al(alc, sizeof(TDerefDeclUsed));
    item->decl = decl;
    item->scope = scope;
    return token_init(alc, tkn_deref_decl_used, item);
}