#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/str.h"
#include "util/vartable.h"
#include <stdio.h>
#include <string.h>

void AFLinit(void) {
    struct data_afl *data = DATA_AFL_GET();
    data->variables = NULL;
}

void AFLfini(void) {}

node_st *AFLprogram(node_st *node) {
    struct data_afl *data = DATA_AFL_GET();
    data->variables = PROGRAM_VARIABLES(node);
    TRAVchildren(node);
    return node;
}

node_st *AFLfundef(node_st *node) {
    struct data_afl *data = DATA_AFL_GET();
    VariableTablePtr old_v = data->variables;
    data->variables = FUNDEF_VARIABLES(node);
    TRAVchildren(node);
    data->variables = old_v;
    return node;
}

node_st *AFLarrexpr(node_st *node) {
    TRAVchildren(node);

    node_st *exprs = ARREXPR_EXPRS(node);

    if (exprs == NULL) return node;

    // Check if any element is an ArrExpr
    bool has_nested = false;
    node_st *curr = exprs;
    while (curr != NULL) {
        if (NODE_TYPE(EXPRS_EXPR(curr)) == NT_ARREXPR) {
            has_nested = true;
            break;
        }
        curr = EXPRS_NEXT(curr);
    }


    if (!has_nested) return node;

    // Flatten nested ArrExprs
    node_st *first = NULL;
    node_st *last = NULL;

    curr = exprs;
    while (curr != NULL) {
        node_st *expr = EXPRS_EXPR(curr);
        EXPRS_EXPR(curr) = NULL; // Take ownership
        if (NODE_TYPE(expr) == NT_ARREXPR) {
            // Take children and append
            node_st *nested_exprs = ARREXPR_EXPRS(expr);
            ARREXPR_EXPRS(expr) = NULL; // Take ownership
            
            if (first == NULL) {
                first = nested_exprs;
            } else {
                EXPRS_NEXT(last) = nested_exprs;
            }

            // Find new last
            node_st *find_last = nested_exprs;
            while (EXPRS_NEXT(find_last) != NULL) {
                find_last = EXPRS_NEXT(find_last);
            }
            last = find_last;
            
            CCNfree(expr);
        } else {
            // Just a regular expression
            node_st *new_node = ASTexprs(expr, NULL);
            
            if (first == NULL) {
                first = new_node;
                last = new_node;
            } else {
                EXPRS_NEXT(last) = new_node;
                last = new_node;
            }
        }
        node_st *tmp = curr;
        curr = EXPRS_NEXT(curr);
        EXPRS_NEXT(tmp) = NULL;
        CCNfree(tmp);
    }

    // Replace the exprs list
    ARREXPR_EXPRS(node) = first;
    EXPR_DIMENSIONEN(node) = 1;

    return node;
}

node_st *AFLvar(node_st *node) {
    struct data_afl *data = DATA_AFL_GET();
    TRAVchildren(node);

    node_st *exprs = VAR_EXPRS(node);
    if (exprs == NULL || EXPRS_NEXT(exprs) == NULL) {
        // No or single dimension - no flattening needed
        return node;
    }

    // Flatten: a[e0, e1, ..., en] -> a[(((e0 * dim1) + e1) * dim2) + e2 ...]
    char *name = VAR_NAME(node);

    // Find the array variable's position in the table to locate its dim entries
    // The dim entries are stored at array_idx+1, array_idx+2, etc.
    int array_idx = -1;
    VariableTable *found_table = NULL;
    {
        VariableTable *curr = data->variables;
        while (curr != NULL) {
            for (int i = 0; i < curr->size; i++) {
                if (STReq(name, curr->variables[i].name) && curr->variables[i].dim > 0) {
                    array_idx = i;
                    found_table = curr;
                    break;
                }
            }
            if (array_idx >= 0) break;
            curr = curr->parent;
        }
    }

    node_st *current_index = EXPRS_EXPR(exprs);
    EXPRS_EXPR(exprs) = NULL; // Take ownership
    node_st *next_exprs = EXPRS_NEXT(exprs);
    EXPRS_NEXT(exprs) = NULL;
    CCNfree(exprs);

    int dim_idx = 1;
    while (next_exprs != NULL) {
        // Create dimension variable reference
        // Use the actual dim variable name from the table
        char *dim_name = NULL;
        if (found_table != NULL && array_idx >= 0 && 
            (array_idx + 1 + dim_idx) < found_table->size) {
            dim_name = found_table->variables[array_idx + 1 + dim_idx].name;
        }
        
        // Fallback to synthesized name
        char buf[256];
        if (dim_name == NULL) {
            snprintf(buf, sizeof(buf), "__%s_dim_%d", name, dim_idx);
            dim_name = buf;
        }
        
        node_st *dim_var = ASTvar(NULL, STRcpy(dim_name));
        VAR_VARPTR(dim_var) = return_varref_ignore_valid(data->variables, dim_name);
        VAR_DIMENSIONEN(dim_var) = 0;
        
        node_st *dim_var_ref = ASTvarref(dim_var);
        EXPR_TYPE(dim_var_ref) = TY_int;
        EXPR_DIMENSIONEN(dim_var_ref) = 0;

        // acc = acc * dim + enext
        node_st *mul = ASTbinop(current_index, dim_var_ref, BO_mul);
        EXPR_TYPE(mul) = TY_int;
        
        node_st *next_expr = EXPRS_EXPR(next_exprs);
        EXPRS_EXPR(next_exprs) = NULL;
        
        node_st *add = ASTbinop(mul, next_expr, BO_add);
        EXPR_TYPE(add) = TY_int;
        
        current_index = add;
        
        // Advance
        node_st *tmp = next_exprs;
        next_exprs = EXPRS_NEXT(next_exprs);
        EXPRS_NEXT(tmp) = NULL;
        CCNfree(tmp);
        dim_idx++;
    }

    VAR_EXPRS(node) = ASTexprs(current_index, NULL);
    VAR_DIMENSIONEN(node) = 1;

    return node;
}
