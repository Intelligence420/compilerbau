#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "ccngen/trav_data.h"

#include "util/funtable.h"
#include "util/print.h"
#include "util/vartable.h"
#include "util/string.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

void CGNinit(void) {
    struct data_cgn *data = DATA_CGN_GET();
    if (global.output_file) {
        data->file = fopen(global.output_file, "w");
        if (data -> file == NULL) {
           CTI(CTI_ERROR, true, "failed to open file %s", global.output_file);
            CTIabortOnError();
        }
    } else {
        data->file = stdout;
    }

    return;
}

void CGNfini(void){
    struct data_cgn *data = DATA_CGN_GET();
    if (global.output_file) {
        file f = data->file;
        fclose(f);
    }
    return;
}

node_st *CGNprogram(node_st *node) {
    TRAVchildren(node);

    struct data_cgn *data = DATA_CGN_GET();
    file f = data->file;

    // TODO: CONSTANTS

    // fprintf(f, "%s", data->export);
    // fprintf(f, "%s", data->global);
    // fprintf(f, "%s", data->import);

    return node;
}

node_st *CGNdecls(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *CGNfundef (node_st *node){
    file f = DATA_CGN_GET()-> file;
    fprintf(f, "%s:\n", FUNHEADER_NAME(FUNDEF_HEADER(node)));

    struct VariableTable *vars = FUNDEF_VARIABLES(node);
    if (vars->capacity - vars->size != 0) {
        fprintf(f, "    esr %ld\n", vars->capacity - vars->size);
    }

    TRAVheader(node);

    node_st *body = FUNDEF_BODY(node);
    TRAVdecl(body);
    TRAVfuns(body);
    TRAVstmts(body);
    

    const char *ty = TYinst(FUNHEADER_TYPE(FUNDEF_HEADER(node)));
    fprintf(f, "    %sreturn\n", ty);
    fprintf(f, "\n");

    return node;
}

node_st *GENfuncall(node_st *node) {
    file f = DATA_CGN_GET()->file;
    struct Function *ref = FUNCALL_REF(node);

    switch (ref->) {
        case CALL_LOCAL: {
            int n = ref->n;
            if (n == -1) {
                fprintf(f, "    isrl\n");
            } else if (n == 0) {
                fprintf(f, "    isr\n");
            } else {
                fprintf(f, "    isrn %d\n", n);
            }

            break;
        }
        case CALL_GLOBAL:
        case CALL_EXTERN: {
            fprintf(f, "    isrg\n");
            break;
        }
    }

    TRAVchildren(node);

    switch (ref->callty) {
        case CALL_LOCAL:
        case CALL_GLOBAL: {
            fprintf(f, "    jsr %ld %s\n", ref->params.arity, ref->label);
            break;
        }
        case CALL_EXTERN: {
            fprintf(f, "    jsre %d\n", ref->n);
            break;
        }
    }

    return node;
}

node_st *CGNfundefs(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CGNfunheader(node_st *node) {
    TRAVparams(node);

    return node;
}

node_st *CGNfunbody(node_st *node) {
    TRAVchildren(node);
    return node;
}

node_st *CGNparams(node_st *node) {
    TRAVparam(node);

    if (PARAMS_PARAMS(node) != NULL) {
        TRAVparams(node);
    }

    return node;
}

node_st *CGNparam(node_st *node) {
    TRAVchildren(node);

    return node;
}

node_st *CGNvardefs(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *CGNvardef (node_st *node) {
    TRAVchildren(node);

    struct data_cgn *data = DATA_CGN_GET();
    file f = data->file;
    
    if (VARDEF_GLOBAL(node)) {
        swritef(data->global, ".global %s\n", VARDEF_TYPE(node));

        if (VARDEF_EXPORT(node)) {
            // Add sting VARDEF_TYPE(node) to data->export
            swritef(data->export, ".exportvar \"%s\" %d\n", VARDEF_NAME(node), VARDEF_L(node));
        }
    }
    
    return node;
}

node_st *CGNvarref(node_st *node) {
    TRAVchildren(node);

    file f = DATA_CGN_GET()->file;

    struct VarReferenz *ref = VAR_VARPTR(VARREF_VAR(node));
    const char *ty = TYinst(ref->type);

    switch (ref->reftype) {
        case REF_LOCAL: {
            if (ref->n == 0) {
                if (ref->l <= 3) {
                    fprintf(f, "    %sload_%d\n", ty, ref->l);
                } else {
                    fprintf(f, "    %sload %d\n", ty, ref->l);
                }
            } else {
                fprintf(f, "    %sloadn %d %d\n", ty, ref->n, ref->l);
            }
            break;
        }
        case REF_GLOBAL: {
            fprintf(f, "    %sloadg %d\n", ty, ref->l);
            break;
        }
        case REF_EXTERN: {
            fprintf(f, "    %sloade %d\n", ty, ref->l);
            break;
        }
    }

    return node;
}

node_st *CGNexprs(node_st *node) {
  TRAVexpr(node);
  for (node_st *next = EXPRS_NEXT(node); next != NULL;
       next = EXPRS_NEXT(next)) {
    TRAVexpr(next);
  }

  return node;
}

node_st *CGNassign (node_st *node){
    TRAVchildren(node);

    file f = DATA_CGN_GET()-> file;

    struct VarReferenz *ref = VAR_VARPTR(ASSIGN_VAR(node));
    const char *type = TYinst(EXPR_TYPE(ASSIGN_EXPR(node)));

    switch (ref->reftype) {
        case REF_LOCAL: {
            if (ref->n == 0) {
                fprintf(f, "    %sstore %d\n", type, ref->l);
            } else {
                fprintf(f, "    %sstore %d %d\n", type, ref->n, ref->l);
            }
            break;
        }
        case REF_GLOBAL: {
            fprintf(f, "    %sstoreg %d\n", type, ref->l);
            break;
        }
        case REF_EXTERN: {
            fprintf(f, "    %sstoree %d\n", type, ref->l);
            break;
        }
    }
    return node;
}

node_st *CGNstmts(node_st *node) {
    TRAVstmt(node);
    TRAVnext(node);

    return node;
}

node_st *CGNbinop(node_st *node) {
    TRAVchildren(node);

    const char *ty = TYinst(EXPR_TYPE(BINOP_LEFT(node)));
    char *instr = NULL;

    switch (BINOP_OP(node)) {
        case BO_add:
            instr = "add";
            break;
        case BO_sub:
            instr = "sub";
            break;
        case BO_mul:
            instr = "mul";
            break;
        case BO_div:
            instr = "div";
            break;
        case BO_mod:
            instr = "rem";
            break;
        case BO_lt:
            instr = "lt";
            break;
        case BO_le:
            instr = "le";
            break;
        case BO_gt:
            instr = "gt";
            break;
        case BO_ge:
            instr = "ge";
            break;
        case BO_eq:
            instr = "eq";
            break;
        case BO_ne:
            instr = "ne";
            break;
        default:
            DBUG_ASSERT(false, "todo!");
    }

    file f = DATA_CGN_GET()->file;
    fprintf(f, "    %s%s\n", ty, instr);

    return node;
}

node_st *CGNnum(node_st *node) {
    TRAVchildren(node);

    file f = DATA_CGN_GET()->file;
    switch (NUM_VAL(node)) {
        case -1: {
            fprintf(f, "    iloadc_m0\n");
            break;
        }
        case 0: {
            fprintf(f, "    iloadc_0\n");
            break;
        }
        case 1: {
            fprintf(f, "    iloadc_1\n");
            break;
        }
        default: {
            size_t ctentry = NUM_G(node);
            fprintf(f, "    iloadc %ld\n", ctentry);
            break;
        }
    }

    return node;
}

node_st *CGNfloat(node_st *node) {
    TRAVchildren(node);

    file f = DATA_CGN_GET()-> file;
    int val = FLOAT_VAL(node);

    if (val == 0.0) {
        fprintf(f, "    floadc_0\n");
    } else if (val == 1.0) {
        fprintf(f, "    floadc_1\n");
    } else {
        size_t ctentry = FLOAT_G(node);
        fprintf(f, "    floadc %ld\n", ctentry);
    }

    return node;
}


node_st *CGNbool(node_st *node) {
    TRAVchildren(node);

    file f = DATA_CGN_GET()->file;
    if (BOOL_VAL(node)) {
        fprintf(f, "    bloadc_t\n");
    } else {
        fprintf(f, "    bloadc_f\n");
    }

    return node;
}