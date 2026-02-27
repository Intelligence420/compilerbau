#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"

void VRinit(void) {}
void VRfini(void) {}

node_st *VRprogram(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *VRfundef(node_st *node) {
    TRAVchildren(node);

    node_st *header = FUNDEF_HEADER(node);
    if (FUNHEADER_TYPE(header) == TY_void) {
        node_st *body = FUNDEF_BODY(node);
        node_st *stmts = FUNBODY_STMTS(body);
        
        bool has_return = false;
        if (stmts != NULL) {
            node_st *curr = stmts;
            while (STMTS_NEXT(curr) != NULL) {
                curr = STMTS_NEXT(curr);
            }
            if (NODE_TYPE(STMTS_STMT(curr)) == NT_RETURNSTATEMENT) {
                has_return = true;
            }
        }
        
        if (has_return == false) {
            node_st *new_return = ASTreturnstatement(NULL);
            node_st *new_stmts = ASTstmts(new_return, NULL);
            
            if (stmts == NULL) {
                FUNBODY_STMTS(body) = new_stmts;
            } else {
                node_st *curr = stmts;
                while (STMTS_NEXT(curr) != NULL) {
                    curr = STMTS_NEXT(curr);
                }
                STMTS_NEXT(curr) = new_stmts;
            }
        }
    }

    return node;
}
