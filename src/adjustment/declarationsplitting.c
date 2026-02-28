#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/str.h"
#include "util/vartable.h"
#include "util/funtable.h"
#include <stddef.h>

void DSinit(void) {
    struct data_ds *data = DATA_DS_GET();
    data->variables = NULL;
}

void DSfini(void) {}

node_st *DSprogram(node_st *node) {
    struct data_ds *data = DATA_DS_GET();
    data->variables = PROGRAM_VARIABLES(node);

    node_st *global_assigns_first = NULL;
    node_st *global_assigns_last = NULL;

    node_st *decls = PROGRAM_DECLS(node);
    while (decls != NULL) {
        node_st *decl = DECLS_DECL(decls);
        if (NODE_TYPE(decl) == NT_VARDEF) {
            if (VARDEF_EXPR(decl) != NULL) {
                node_st *expr = VARDEF_EXPR(decl);
                VARDEF_EXPR(decl) = NULL;

                // Create Var node for the left-hand side of the assignment
                node_st *var = ASTvar(NULL, STRcpy(VARDEF_NAME(decl)));
                VAR_VARPTR(var) = return_varref_ignore_valid(data->variables, VARDEF_NAME(decl));
                VAR_DIMENSIONEN(var) = 0;

                // Create the assignment statement
                node_st *assign = ASTassign(var, expr);
                node_st *stmt = ASTstmts(assign, NULL);

                if (global_assigns_first == NULL) {
                    global_assigns_first = stmt;
                    global_assigns_last = stmt;
                } else {
                    STMTS_NEXT(global_assigns_last) = stmt;
                    global_assigns_last = stmt;
                }
            }
        }
        decls = DECLS_NEXT(decls);
    }

    TRAVchildren(node);

    // After traversing children, if we have global assignments, create the __init function
    if (global_assigns_first != NULL) {
        node_st *header = ASTfunheader(NULL, TY_void, STRcpy("__init"));
        node_st *body = ASTfunbody(NULL, NULL, global_assigns_first);
        node_st *fundef = ASTfundef(header, body, true); // export = true
        
        // Initialize tables to avoid segfault in MemFree
        FUNDEF_FUNCTIONS(fundef) = create_funtable(PROGRAM_FUNCTIONS(node));
        FUNDEF_VARIABLES(fundef) = create_vartable(PROGRAM_VARIABLES(node));

        // Append __init to the program's declarations
        node_st *new_decl_node = ASTdecls(fundef, NULL);
        node_st *curr_decls = PROGRAM_DECLS(node);
        if (curr_decls == NULL) {
            PROGRAM_DECLS(node) = new_decl_node;
        } else {
            while (DECLS_NEXT(curr_decls) != NULL) {
                curr_decls = DECLS_NEXT(curr_decls);
            }
            DECLS_NEXT(curr_decls) = new_decl_node;
        }
    }

    return node;
}

node_st *DSfundef(node_st *node) {
    struct data_ds *data = DATA_DS_GET();
    VariableTable *old_v = data->variables;
    data->variables = FUNDEF_VARIABLES(node);
    TRAVchildren(node);
    data->variables = old_v;
    return node;
}

node_st *DSfunbody(node_st *node) {
    struct data_ds *data = DATA_DS_GET();

    node_st *local_assigns_first = NULL;
    node_st *local_assigns_last = NULL;

    node_st *vardefs = FUNBODY_DECL(node);
    while (vardefs != NULL) {
        node_st *vardef = VARDEFS_DEC(vardefs);
        if (VARDEF_EXPR(vardef) != NULL) {
            node_st *expr = VARDEF_EXPR(vardef);
            VARDEF_EXPR(vardef) = NULL;

            // Create Var node for the left-hand side of the assignment
            node_st *var = ASTvar(NULL, STRcpy(VARDEF_NAME(vardef)));
            VAR_VARPTR(var) = return_varref_ignore_valid(data->variables, VARDEF_NAME(vardef));
            VAR_DIMENSIONEN(var) = 0;

            // Create the assignment statement
            node_st *assign = ASTassign(var, expr);
            node_st *stmt = ASTstmts(assign, NULL);

            if (local_assigns_first == NULL) {
                local_assigns_first = stmt;
                local_assigns_last = stmt;
            } else {
                STMTS_NEXT(local_assigns_last) = stmt;
                local_assigns_last = stmt;
            }
        }
        vardefs = VARDEFS_NEXT(vardefs);
    }

    // Prepend collected assignments to the existing statements in the function body
    if (local_assigns_first != NULL) {
        node_st *existing_stmts = FUNBODY_STMTS(node);
        STMTS_NEXT(local_assigns_last) = existing_stmts;
        FUNBODY_STMTS(node) = local_assigns_first;
    }

    TRAVchildren(node);
    return node;
}
