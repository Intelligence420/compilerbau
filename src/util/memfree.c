#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "util/funtable.h"

node_st *FREEprogram(node_st *node) {
    TRAVchildren(node);
    table_free(PROGRAM_FUNCTIONS(node));
    return node;
}

node_st *FREEfundef(node_st *node) {
    TRAVchildren(node);
    table_free(FUNDEF_FUNCTIONS(node));
    return node;
}
