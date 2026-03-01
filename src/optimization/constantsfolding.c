#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "palm/ctinfo.h"
#include "util/vartable.h"
#include "user_types.h"
#include "global/globals.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * Reads the given line number from the source file.
 * Caller is responsible for freeing the returned string.
 * Returns NULL if the file cannot be opened or the line does not exist.
 */
static char *read_source_line(int line_num) {
    if (!global.input_file || line_num <= 0) return NULL;
    FILE *f = fopen(global.input_file, "r");
    if (!f) return NULL;

    char buf[1024];
    int current = 0;
    while (fgets(buf, sizeof(buf), f)) {
        current++;
        if (current == line_num) {
            fclose(f);
            int len = (int)strlen(buf);
            if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
            return STRcpy(buf);
        }
    }
    fclose(f);
    return NULL;
}

/**
 * Reports a division-by-zero error using CTIobj so it prints with
 * the source line and a caret pointing to the operator, just like
 * a syntax error from the parser.
 */
static void report_div_by_zero(node_st *node, const char *op_name) {
    int line   = NODE_BLINE(node);
    int col    = NODE_BCOL(node);
    int ecol   = NODE_ECOL(node);
    char *src_line = read_source_line(line);

    struct ctinfo info = {
        .first_line   = line,
        .first_column = col,
        .last_line    = line,
        .last_column  = (ecol > 0 ? ecol : col),
        .filename     = global.input_file,
        .line         = src_line
    };

    CTIobj(CTI_WARN, true, info, "%s by zero in constant expression.", op_name);

    if (src_line) MEMfree(src_line);
}

/**
 * Constant Folding Breakdown:
 * 
 * Done:
 * - Basic integer arithmetic (+, -, *, /, %) on constant Num nodes.
 * - Basic float arithmetic (+, -, *, /) on constant Float nodes.
 * - Boolean logical operations (&&, ||) on constant Bool nodes.
 * - Comparison operations (==, !=, <, <=, >, >=) for int, float, and bool.
 * - Unary operations: arithmetic negation (-) for int/float, logical not (!) for bool.
 * - Type casts between int, float, and bool constants.
 * 
 * TODO:
 * - Identity optimizations (e.g., x + 0 -> x, x * 1 -> x, x * 0 -> 0).
 * - Constant propagation (requires tracking variable values).
 * - Dead branch elimination (e.g., if (true) { ... } else { ... } -> { ... }).
 * - Folding for array expressions if all elements are constants.
 * - Handle potential overflow/underflow warnings.
 */



void CFinit(void) {}
void CFfini(void) {}

node_st *CFbinop(node_st *node) {
    TRAVchildren(node);

    node_st *left = BINOP_LEFT(node);
    node_st *right = BINOP_RIGHT(node);

    if (NODE_TYPE(left) == NT_NUM && NODE_TYPE(right) == NT_NUM) {
        int lval = NUM_VAL(left);
        int rval = NUM_VAL(right);
        int res = 0;
        bool res_bool = false;
        bool is_bool = false;
        bool foldable = true;

        switch (BINOP_OP(node)) {
        case BO_add: res = lval + rval; break;
        case BO_sub: res = lval - rval; break;
        case BO_mul: res = lval * rval; break;
        case BO_div: 
            if (rval == 0) {
                report_div_by_zero(node, "Division");
                foldable = false;
            } else res = lval / rval; 
            break;
        case BO_mod:
            if (rval == 0) {
                report_div_by_zero(node, "Modulo");
                foldable = false;
            } else res = lval % rval;
            break;
        case BO_lt: res_bool = (lval < rval); is_bool = true; break;
        case BO_le: res_bool = (lval <= rval); is_bool = true; break;
        case BO_gt: res_bool = (lval > rval); is_bool = true; break;
        case BO_ge: res_bool = (lval >= rval); is_bool = true; break;
        case BO_eq: res_bool = (lval == rval); is_bool = true; break;
        case BO_ne: res_bool = (lval != rval); is_bool = true; break;
        default: foldable = false; break;
        }

        if (foldable) {
            node_st *res_node;
            if (is_bool) {
                res_node = ASTbool(res_bool);
                EXPR_TYPE(res_node) = TY_bool;
            } else {
                res_node = ASTnum(res);
                EXPR_TYPE(res_node) = TY_int;
            }
            CCNfree(node);
            return res_node;
        }
    } else if (NODE_TYPE(left) == NT_FLOAT && NODE_TYPE(right) == NT_FLOAT) {
        float lval = FLOAT_VAL(left);
        float rval = FLOAT_VAL(right);
        float res = 0.0;
        bool res_bool = false;
        bool is_bool = false;
        bool foldable = true;

        switch (BINOP_OP(node)) {
        case BO_add: res = lval + rval; break;
        case BO_sub: res = lval - rval; break;
        case BO_mul: res = lval * rval; break;
        case BO_div: 
            if (rval == 0.0) {
                report_div_by_zero(node, "Division");
                foldable = false;
            } else res = lval / rval; 
            break;
        case BO_lt: res_bool = (lval < rval); is_bool = true; break;
        case BO_le: res_bool = (lval <= rval); is_bool = true; break;
        case BO_gt: res_bool = (lval > rval); is_bool = true; break;
        case BO_ge: res_bool = (lval >= rval); is_bool = true; break;
        case BO_eq: res_bool = (lval == rval); is_bool = true; break;
        case BO_ne: res_bool = (lval != rval); is_bool = true; break;
        default: foldable = false; break;
        }

        if (foldable) {
            node_st *res_node;
            if (is_bool) {
                res_node = ASTbool(res_bool);
                EXPR_TYPE(res_node) = TY_bool;
            } else {
                res_node = ASTfloat(res);
                EXPR_TYPE(res_node) = TY_float;
            }
            CCNfree(node);
            return res_node;
        }
    } else if (NODE_TYPE(left) == NT_BOOL && NODE_TYPE(right) == NT_BOOL) {
        bool lval = BOOL_VAL(left);
        bool rval = BOOL_VAL(right);
        bool res = false;
        bool foldable = true;

        switch (BINOP_OP(node)) {
        case BO_and: 
        case BO_mul: res = lval && rval; break;
        case BO_or:
        case BO_add: res = lval || rval; break;
        case BO_eq: res = lval == rval; break;
        case BO_ne: res = lval != rval; break;
        default: foldable = false; break;
        }


        if (foldable) {
            node_st *res_node = ASTbool(res);
            EXPR_TYPE(res_node) = TY_bool;
            CCNfree(node);
            return res_node;
        }
    }

    return node; 
}

node_st *CFmonop(node_st *node) {
    TRAVchildren(node);
    node_st *expr = MONOP_EXPR(node);

    if (NODE_TYPE(expr) == NT_NUM) {
        if (MONOP_OP(node) == MO_neg) {
            node_st *res = ASTnum(-NUM_VAL(expr));
            EXPR_TYPE(res) = TY_int;
            CCNfree(node);
            return res;
        }
    } else if (NODE_TYPE(expr) == NT_FLOAT) {
        if (MONOP_OP(node) == MO_neg) {
            node_st *res = ASTfloat(-FLOAT_VAL(expr));
            EXPR_TYPE(res) = TY_float;
            CCNfree(node);
            return res;
        }
    } else if (NODE_TYPE(expr) == NT_BOOL) {
        if (MONOP_OP(node) == MO_not) {
            node_st *res = ASTbool(!BOOL_VAL(expr));
            EXPR_TYPE(res) = TY_bool;
            CCNfree(node);
            return res;
        }
    }

    return node;
}

node_st *CFtypecast(node_st *node) {
    TRAVchildren(node);
    node_st *expr = TYPECAST_EXPR(node);
    enum DeclarationType target_type = TYPECAST_TYPE(node);

    if (NODE_TYPE(expr) == NT_NUM) {
        int val = NUM_VAL(expr);
        if (target_type == TY_float) {
            node_st *res = ASTfloat((float)val);
            EXPR_TYPE(res) = TY_float;
            CCNfree(node);
            return res;
        } else if (target_type == TY_bool) {
            node_st *res = ASTbool(val != 0);
            EXPR_TYPE(res) = TY_bool;
            CCNfree(node);
            return res;
        } else if (target_type == TY_int) {
            // redundant cast, but let's handle it
            node_st *res = ASTnum(val);
            EXPR_TYPE(res) = TY_int;
            CCNfree(node);
            return res;
        }
    } else if (NODE_TYPE(expr) == NT_FLOAT) {
        float val = FLOAT_VAL(expr);
        if (target_type == TY_int) {
            node_st *res = ASTnum((int)val);
            EXPR_TYPE(res) = TY_int;
            CCNfree(node);
            return res;
        } else if (target_type == TY_bool) {
            node_st *res = ASTbool(val != 0.0);
            EXPR_TYPE(res) = TY_bool;
            CCNfree(node);
            return res;
        } else if (target_type == TY_float) {
            node_st *res = ASTfloat(val);
            EXPR_TYPE(res) = TY_float;
            CCNfree(node);
            return res;
        }
    } else if (NODE_TYPE(expr) == NT_BOOL) {
        bool val = BOOL_VAL(expr);
        if (target_type == TY_int) {
            node_st *res = ASTnum(val ? 1 : 0);
            EXPR_TYPE(res) = TY_int;
            CCNfree(node);
            return res;
        } else if (target_type == TY_float) {
            node_st *res = ASTfloat(val ? 1.0 : 0.0);
            EXPR_TYPE(res) = TY_float;
            CCNfree(node);
            return res;
        } else if (target_type == TY_bool) {
            node_st *res = ASTbool(val);
            EXPR_TYPE(res) = TY_bool;
            CCNfree(node);
            return res;
        }
    }

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
