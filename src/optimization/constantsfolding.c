#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/vartable.h"
#include "user_types.h"

void CFinit(void) {}
void CFfini(void) {}

node_st *CFbinop(node_st *node) {
    TRAVchildren(node);
    return node; 
}

node_st *CFmonop(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFtypecast(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFassign(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFvardef(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFreturnstatement(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFnum(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFfloat(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFbool(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFfuncall(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFvarref(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFarrexpr(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFparam(node_st *node) {
    TRAVchildren(node);
    return node;
}


node_st *CFifstatement(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFwhilestatement(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFdostatement(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFforstatement(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CFfuncallstmt(node_st *node) {
    TRAVchildren(node);
    return node;
}
