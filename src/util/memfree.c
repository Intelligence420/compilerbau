#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "util/funtable.h"
#include "util/vartable.h"

node_st *FREEprogram(node_st *node) {
    TRAVchildren(node);
    funtable_free(PROGRAM_FUNCTIONS(node));
    vartable_free(PROGRAM_VARIABLES(node));
    return node;
}

node_st *FREEfundef(node_st *node) {
    TRAVchildren(node);
    funtable_free(FUNDEF_FUNCTIONS(node));
    vartable_free(FUNDEF_VARIABLES(node));
    return node;
}

