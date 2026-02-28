#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/consttable.h"
#include "util/funtable.h"
#include "util/vartable.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

/** Ausgabedatei für Assembly */
static FILE *outfile = NULL;

/* Zähler .importfun erhält Index (0, 1, 2, ...) */
static int import_fun_counter = 0;

/* Zähler .importvar erhält Index (0, 1, 2, ...) */
static int import_var_counter = 0;

/* Zähler .global erhält Index (0, 1, 2, ...) */
static int global_var_counter = 0;

/* Gibt Zeile Assembly-Code in Ausgabedatei aus */
static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(outfile, "    ");
    vfprintf(outfile, fmt, args);
    fprintf(outfile, "\n");
    va_end(args);
}

/* Gibt Label in die Ausgabedatei aus */
static void emit_label(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(outfile, fmt, args);
    fprintf(outfile, ":\n");
    va_end(args);
}

/* Gibt eine Pseudo-Instruktion aus */
static void emit_pseudo(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(outfile, fmt, args);
    fprintf(outfile, "\n");
    va_end(args);
}

/* Zähler für Index Label-Namen */
static int new_label(struct data_cgn *data) {
    return data->label_counter++;
}

/* Typ-Prefix für Assembly-Instruktionen
   VM-Spezifikation: 
   - 'i' für int    
   - 'f' für float  
   - 'b' für bool   
   - 'a' für array  */
static char type_prefix(enum DeclarationType type) {
    switch (type) {
    case TY_int:   return 'i';
    case TY_float: return 'f';
    case TY_bool:  return 'b';
    default:       return '?'; // Sollte nicht auftreten
    }
}

/* Typ-String für Pseudo-Instruktionen *
   Verwendet in .const, .global, .importvar, .exportfun, .importfun
   Mögliche Werte: "int", "float", "bool", "int[]", "float[]", "bool[]", "void" */
static const char *type_string(enum DeclarationType type) {
    switch (type) {
    case TY_int:   return "int";
    case TY_float: return "float";
    case TY_bool:  return "bool";
    case TY_void:  return "void";
    default:       return "???";
    }
}

/* Erzeugt Signatur-String für Funktion für .exportfun/.importfun
   VM-Spezifikation Assembly Format ( 2.1.2):
    FunctionSignature => Name RetType Type*
    Name => "name" */
static void emit_function_signature(node_st *header_node, bool is_import) {
    const char *name = FUNHEADER_NAME(header_node);
    const char *ret_type = type_string(FUNHEADER_TYPE(header_node));

    if (is_import) {
        fprintf(outfile, ".importfun \"%s\" %s", name, ret_type);
    } else {
        // Für exportfun: .exportfun "name" rettype paramtypes... label
        // Das Label ist bei uns identisch mit dem (ggf. umbenannten) Namen
        fprintf(outfile, ".exportfun \"%s\" %s", name, ret_type);
    }

    node_st *params = FUNHEADER_PARAMS(header_node);
    while (params != NULL) {
        node_st *param = PARAMS_PARAM(params);
        node_st *ids = PARAM_IDS(param);
        int dims = 0;
        node_st *ids_tmp = ids;
        while (ids_tmp != NULL) {
            dims++;
            fprintf(outfile, " int");
            ids_tmp = IDS_NEXT(ids_tmp);
        }
        if (dims > 0) {
            fprintf(outfile, " %s[]", type_string(PARAM_TYPE(param)));
        } else {
            fprintf(outfile, " %s", type_string(PARAM_TYPE(param)));
        }
        params = PARAMS_PARAMS(params);
    }

    if (!is_import) {
        fprintf(outfile, " %s", name);
    }
    fprintf(outfile, "\n");
}

/* Zählt die "flache" Anzahl der Argumente einer Funktion. */
static int count_flat_params(node_st *params_node) {
    int count = 0;
    while (params_node != NULL) {
        node_st *param = PARAMS_PARAM(params_node);
        count++; // Der Parameter selbst
        // Dimensions-Parameter zählen
        node_st *ids = PARAM_IDS(param);
        while (ids != NULL) {
            count++;
            ids = IDS_NEXT(ids);
        }
        params_node = PARAMS_PARAMS(params_node);
    }
    return count;
}

/* Init/Fini */
void CGNinit(void) {
    if (global.output_file != NULL) {
        outfile = fopen(global.output_file, "w");
        if (outfile == NULL) {
            CTI(CTI_ERROR, true, "Cannot open output file %s", global.output_file);
        }
    } else {
        outfile = stdout;
    }

    import_fun_counter = 0;
    import_var_counter = 0;
    global_var_counter = 0;
}

void CGNfini(void) {
    if (outfile != NULL && outfile != stdout) {
        fclose(outfile);
    }
    outfile = NULL;
}

/* CGNprogram 
   VM-Spezifikation (Abschnitt 2.2.1 - Order of operation):
    Die VM Schrittfolge:
        1. Module laden und verifizieren
        2. Global-Tabelle initialisieren (Typen, nicht Werte)
        3. Imports patchen (alle Module nach Matches durchsuchen)
        4. __init-Funktion jedes Moduls ausführen (falls vorhanden)
        5. main finden und ausführen
        6. Return-Wert von main als Exit-Code */
node_st *CGNprogram(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    data->variables = PROGRAM_VARIABLES(node);
    data->functions = PROGRAM_FUNCTIONS(node);
    data->constants = PROGRAM_CONSTANTS(node);
    data->label_counter = 0;
    data->nesting_depth = 0;
    data->store_context = false;

    // Erster Durchlauf - Import/Global-Indizes vergeben
    node_st *decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_FUNDEC) {
            char *name = FUNHEADER_NAME(FUNDEC_HEADER(decl));
            Function *f = funtable_get_function_ptr(data->functions, name);
            if (f) f->assembly_index = import_fun_counter++;
        } else if (NODE_TYPE(decl) == NT_GLOBALDEC) {
            node_st *ids = GLOBALDEC_IDS(decl);
            while (ids != NULL) {
                char *id_name = IDS_ID(ids);
                Variable *v = vartable_get_variable_ptr(data->variables, id_name);
                if (v) v->assembly_index = import_var_counter++;
                ids = IDS_NEXT(ids);
            }
            char *name = GLOBALDEC_NAME(decl);
            Variable *v = vartable_get_variable_ptr(data->variables, name);
            if (v) v->assembly_index = import_var_counter++;
        } else if (NODE_TYPE(decl) == NT_VARDEF) {
            char *name = VARDEF_NAME(decl);
            Variable *v = vartable_get_variable_ptr(data->variables, name);
            if (v) v->assembly_index = global_var_counter++;
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // Code-Generierung durch AST-Traversierung
    TRAVchildren(node);

    // Pseudo-Instruktionen ausgeben
    fprintf(outfile, "\n; --- Pseudo-Instructions ---\n");

    // --- .exportfun ---
    decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_FUNDEF && FUNDEF_EXPORT(decl)) {
            emit_function_signature(FUNDEF_HEADER(decl), false);
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // --- .importfun ---
    decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_FUNDEC) {
            emit_function_signature(FUNDEC_HEADER(decl), true);
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // --- .exportvar ---
    decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_VARDEF && VARDEF_EXPORT(decl)) {
            char *name = VARDEF_NAME(decl);
            Variable *v = vartable_get_variable_ptr(data->variables, name);
            emit_pseudo(".exportvar \"%s\" %d", name, v->assembly_index);
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // --- .importvar ---
    decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_GLOBALDEC) {
            node_st *ids = GLOBALDEC_IDS(decl);
            while (ids != NULL) {
                emit_pseudo(".importvar \"%s\" int", IDS_ID(ids));
                ids = IDS_NEXT(ids);
            }
            char *name = GLOBALDEC_NAME(decl);
            Variable *v = vartable_get_variable_ptr(data->variables, name);
            if (v->dim > 0) {
                emit_pseudo(".importvar \"%s\" %s[]", name, type_string(v->type));
            } else {
                emit_pseudo(".importvar \"%s\" %s", name, type_string(v->type));
            }
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // --- .global ---
    decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_VARDEF) {
            Variable *v = vartable_get_variable_ptr(data->variables, VARDEF_NAME(decl));
            if (v->dim > 0) {
                emit_pseudo(".global %s[]", type_string(v->type));
            } else {
                emit_pseudo(".global %s", type_string(v->type));
            }
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // --- .const ---
    for (int i = 0; i < data->constants->size; i++) {
        Constant c = data->constants->constants[i];
        if (c.type == TY_int) {
            emit_pseudo(".const int %d", c.cint);
        } else if (c.type == TY_float) {
            emit_pseudo(".const float %f", c.cflt);
        } else if (c.type == TY_bool) {
            emit_pseudo(".const bool %s", c.cint ? "true" : "false");
        }
    }

    return node;
}


/* CGNfundef
   Aufgaben:
    1. Label für die Funktion ausgeben (Funktionsname als Label)
    2. Scope wechseln (Variables/Functions auf FunDef-Tabellen setzen)
    3. Nesting-Depth erhöhen
    4. Body traversieren (dort wird esr + Instruktionen generiert)
    5. Scope wiederherstellen
 
  VM-Spezifikation:
    Jede Funktion beginnt mit einem Label, gefolgt von esr (Enter Subroutine).
    Dann folgt der Funktions-Body, der mit einem return-Statement endet. */
node_st *CGNfundef(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();

    // Scope sichern und wechseln
    VariableTablePtr saved_vars = data->variables;
    FunctionTablePtr saved_funs = data->functions;
    enum DeclarationType saved_return = data->return_type;
    int saved_depth = data->nesting_depth;

    data->variables = FUNDEF_VARIABLES(node);
    data->functions = FUNDEF_FUNCTIONS(node);

    node_st *header = FUNDEF_HEADER(node);
    data->return_type = FUNHEADER_TYPE(header);
    data->nesting_depth++;

    // Count param slots
    data->param_slots = count_flat_params(FUNHEADER_PARAMS(header));

    // Alle lokalen Variablen und Parameter-Slots berechnen
    int current_slot = 0;
    for (int i = 0; i < data->variables->size; i++) {
        Variable *v = &data->variables->variables[i];
        v->slot_index = current_slot;
        current_slot += (1 + v->dim);
    }
    data->total_slots = current_slot;

    // Label für die Funktion ausgeben
    fprintf(outfile, "\n"); // Leerzeile vor Funktionen zur besseren Lesbarkeit
    emit_label(FUNHEADER_NAME(header));

    // Body traversieren (erzeugt esr + Instruktionen + return)
    TRAVdo(FUNDEF_BODY(node));

    // Scope wiederherstellen
    data->variables = saved_vars;
    data->functions = saved_funs;
    data->return_type = saved_return;
    data->nesting_depth = saved_depth;

    return node;
}

/* CGNfundec 
   VM-Spezifikation (Abschnitt 2.1.3, .importfun):
    "defines an import entry that should, at load-time, be linked up
     with an external definition of a function" */
node_st *CGNfundec(node_st *node) {
    (void)node;
    return node;
}

/* CGNglobaldec
   VM-Spezifikation (Abschnitt 2.1.3, .importvar):
    "defines an import entry that should, at load-time, be linked up
*    with an external definition of a variable" */
node_st *CGNglobaldec(node_st *node) {
    (void)node;
    return node;
}

/* CGNfunbody
   Aufgaben:
    1. Anzahl lokaler Variablen zählen (VarDefs in FUNBODY_DECL)
       ACHTUNG: Parameter sind NICHT inkludiert, die sind bereits im Frame.
       Aber der esr-Wert muss die Anzahl ALLER lokalen Slots sein,
       also params + locals.
    2. esr-Instruktion ausgeben
    3. Statements traversieren
 
   VM-Spezifikation (esr, Abschnitt 1.4.4):
    "esr L — Enter subroutine. Advances the top of the stack with L elements,
     thus reserving space for L local variables."
    "esr should be issued at the beginning of a subroutine, thus ensuring that
     the locals and arguments are contiguous in the stack"
    "the first argument is addressable as local 0, and so on."
 
  WICHTIG: L bei esr ist die Anzahl der ZUSÄTZLICHEN lokalen Variablen,
  NICHT inklusive der Parameter (die sind bereits im Activation Record). */
node_st *CGNfunbody(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    int local_slots = data->total_slots - data->param_slots;
    if (local_slots < 0) local_slots = 0;

    // esr ausgeben
    emit("esr %d", local_slots);

    // VarDefs traversieren (nur für lokale Array-Allokationen)
    TRAVopt(FUNBODY_DECL(node));

    // Statements traversieren
    TRAVopt(FUNBODY_STMTS(node));

    // Verschachtelte Funktionen traversieren
    TRAVopt(FUNBODY_FUNS(node));

    return node;
}

/* CGNvardef  */
node_st *CGNvardef(node_st *node) {
    if (VARDEF_GLOBAL(node)) {
        return node;
    }
    struct data_cgn *data = DATA_CGN_GET();

    // Lokale Variable: Arrays allokieren
    node_st *dim_exprs = VARDEF_EXPRS(node);
    if (dim_exprs != NULL) {
        // Dimensionen evaluieren
        TRAVopt(VARDEF_EXPRS(node));

        Variable *v = vartable_get_variable_ptr(data->variables, VARDEF_NAME(node));
        if (v == NULL) return node;

        // Inewa (wir gehen von 1D aus)
        emit("%cnewa", type_prefix(v->type));
        
        // Speichern
        emit("astore %d", v->slot_index);
    }

    return node;
}

/* Statements */
node_st *CGNassign(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    TRAVdo(ASSIGN_EXPR(node)); // RHS auf Stack

    node_st *var = ASSIGN_LET(node);
    if (var == NULL) return node;

    VarReferenz *ref = VAR_VARPTR(var);
    if (ref == NULL) return node;

    Variable *v_info = vartable_get_variable_ptr(data->variables, VAR_NAME(var));
    int slot = v_info ? v_info->slot_index : ref->l;

    char prefix = type_prefix(ref->type);
    if (VAR_EXPRS(var) != NULL) {
        TRAVdo(VAR_EXPRS(var)); // Index
        if (ref->reftype == REF_EXTERN) {
            emit("aloade %d", v_info->assembly_index);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("aloadg %d", v_info->assembly_index);
        } else {
            if (ref->n > 0) emit("aloadn %d %d", ref->n, slot);
            else emit("aload %d", slot);
        }
        emit("%cstorea", prefix);
    } else {
        char full_prefix = (ref->dim > 0) ? 'a' : prefix;
        if (ref->reftype == REF_EXTERN) {
            emit("%cstoree %d", full_prefix, v_info->assembly_index);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("%cstoreg %d", full_prefix, v_info->assembly_index);
        } else {
            if (ref->n > 0) emit("%cstoren %d %d", full_prefix, ref->n, slot);
            else emit("%cstore %d", full_prefix, slot);
        }
    }
    return node;
}

node_st *CGNfuncallstmt(node_st *node) {
    TRAVdo(FUNCALLSTMT_CALL(node));
    node_st *call = FUNCALLSTMT_CALL(node);
    Function *fun = FUNCALL_FUNPTR(call);
    if (fun != NULL && fun->return_type != TY_void) {
        emit("%cpop", type_prefix(fun->return_type));
    }
    return node;
}

node_st *CGNifstatement(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    int label_else = new_label(data);
    int label_end = new_label(data);

    TRAVdo(IFSTATEMENT_EXPR(node)); // Condition

    if (IFSTATEMENT_ELSEBLOCK(node) != NULL) {
        emit("branch_f else_%d", label_else);
        TRAVdo(IFSTATEMENT_BLOCK(node));
        emit("jump end_%d", label_end);
        emit_label("else_%d", label_else);
        TRAVopt(IFSTATEMENT_ELSEBLOCK(node));
        emit_label("end_%d", label_end);
    } else {
        emit("branch_f end_%d", label_end);
        TRAVdo(IFSTATEMENT_BLOCK(node));
        emit_label("end_%d", label_end);
    }
    return node;
}

node_st *CGNdostatement(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    int label_loop = new_label(data);
    emit_label("loop_%d", label_loop);
    TRAVopt(DOSTATEMENT_BLOCK(node));
    TRAVdo(DOSTATEMENT_EXPR(node));
    emit("branch_t loop_%d", label_loop);
    return node;
}

node_st *CGNreturnstatement(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    if (RETURNSTATEMENT_EXPR(node) != NULL) {
        TRAVdo(RETURNSTATEMENT_EXPR(node));
        emit("%creturn", type_prefix(data->return_type));
    } else {
        emit("return");
    }
    return node;
}

/* Ausdrücke */
node_st *CGNvarref(node_st *node) {
    TRAVdo(VARREF_VAR(node));
    return node;
}

node_st *CGNvar(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    VarReferenz *ref = VAR_VARPTR(node);
    if (ref == NULL) return node;

    Variable *v_info = vartable_get_variable_ptr(data->variables, VAR_NAME(node));
    int slot = v_info ? v_info->slot_index : ref->l;

    char prefix = type_prefix(ref->type);
    bool is_array_access = (VAR_EXPRS(node) != NULL);

    if (is_array_access) {
        TRAVdo(VAR_EXPRS(node));
        if (ref->reftype == REF_EXTERN) {
            emit("aloade %d", v_info->assembly_index);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("aloadg %d", v_info->assembly_index);
        } else {
            if (ref->n > 0) emit("aloadn %d %d", ref->n, slot);
            else emit("aload %d", slot);
        }
        emit("%cloada", prefix);
    } else {
        char full_prefix = (ref->dim > 0) ? 'a' : prefix;
        if (ref->reftype == REF_EXTERN) {
            emit("%cloade %d", full_prefix, v_info->assembly_index);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("%cloadg %d", full_prefix, v_info->assembly_index);
        } else {
            if (ref->n > 0) emit("%cloadn %d %d", full_prefix, ref->n, slot);
            else emit("%cload %d", full_prefix, slot);
        }
    }
    return node;
}

node_st *CGNbinop(node_st *node) {
    TRAVdo(BINOP_LEFT(node));
    TRAVdo(BINOP_RIGHT(node));
    enum BinOpType op = BINOP_OP(node);
    enum DeclarationType type = EXPR_TYPE(BINOP_LEFT(node));
    char prefix = type_prefix(type);

    switch (op) {
    case BO_add: if (type == TY_bool) emit("badd"); else emit("%cadd", prefix); break;
    case BO_sub: emit("%csub", prefix); break;
    case BO_mul: if (type == TY_bool) emit("bmul"); else emit("%cmul", prefix); break;
    case BO_div: emit("%cdiv", prefix); break;
    case BO_mod: emit("irem"); break;
    case BO_lt:  emit("%clt",  prefix); break;
    case BO_le:  emit("%cle",  prefix); break;
    case BO_gt:  emit("%cgt",  prefix); break;
    case BO_ge:  emit("%cge",  prefix); break;
    case BO_eq:  emit("%ceq",  prefix); break;
    case BO_ne:  emit("%cne",  prefix); break;
    case BO_and: emit("bmul"); break;
    case BO_or:  emit("badd"); break;
    default: break;
    }
    return node;
}

node_st *CGNmonop(node_st *node) {
    TRAVdo(MONOP_EXPR(node));
    if (MONOP_OP(node) == MO_neg) {
        emit("%cneg", type_prefix(EXPR_TYPE(MONOP_EXPR(node))));
    } else {
        emit("bnot");
    }
    return node;
}

node_st *CGNfuncall(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    Function *fun = funtable_get_function_ptr(data->functions, FUNCALL_NAME(node));
    if (fun == NULL) {
        fun = FUNCALL_FUNPTR(node);
    }
    if (fun == NULL) return node;
    emit("isrg");
    TRAVopt(FUNCALL_EXPRS(node));
    int flat_args = 0;
    const char *target_name = fun->name;
    node_st *fundef_node = (node_st *)fun->fundef_node;

    if (fundef_node != NULL) {
        node_st *header = NULL;
        if (NODE_TYPE(fundef_node) == NT_FUNDEF) {
            header = FUNDEF_HEADER(fundef_node);
        } else if (NODE_TYPE(fundef_node) == NT_FUNDEC) {
            header = FUNDEC_HEADER(fundef_node);
        }

        if (header != NULL) {
            flat_args = count_flat_params(FUNHEADER_PARAMS(header));
            target_name = FUNHEADER_NAME(header);
        }
    }

    if (flat_args == 0) {
        for (int i = 0; i < fun->params.arity; i++) {
            flat_args += (1 + fun->params.list[i].dim);
        }
    }

    if (fun->isextern) emit("jsre %d", fun->assembly_index);
    else emit("jsr %d %s", flat_args, target_name);
    return node;
}

node_st *CGNnum(node_st *node) {
    emit("iloadc %d", NUM_G(node));
    return node;
}

node_st *CGNfloat(node_st *node) {
    emit("floadc %d", FLOAT_G(node));
    return node;
}

node_st *CGNbool(node_st *node) {
    if (BOOL_VAL(node)) emit("bloadc_t"); else emit("bloadc_f");
    return node;
}

node_st *CGNtypecast(node_st *node) {
    TRAVdo(TYPECAST_EXPR(node));
    if (TYPECAST_TYPE(node) == TY_float) emit("i2f");
    else emit("f2i");
    return node;
}

node_st *CGNarrexpr(node_st *node) {
    TRAVdo(ARREXPR_EXPRS(node));
    emit("%cnewa", type_prefix(EXPR_TYPE(node)));
    return node;
}

/*ZUSÄTZLICHE HINWEISE UND OFFENE PUNKTE */

/*
 * TODO: [WICHTIG] Import-Index-Tracking implementieren
 *   Problem: jsre benötigt den Index in der Import-Tabelle.
 *   Die Import-Tabelle wird durch die Reihenfolge der .importfun
 *   Pseudo-Instruktionen definiert.
 *   → Einfaches Array/Map: Funktionsname → Import-Index
 *   Gleiches gilt für .importvar → Variable-Import-Index
 *
 * TODO: [WICHTIG] Global-Variable-Index-Tracking implementieren
 *   Problem: iloadg/istoreg benötigen den Index in der Global-Tabelle.
 *   Die Global-Tabelle wird durch die Reihenfolge der .global
 *   Pseudo-Instruktionen definiert.
 *   → Die VarReferenz->l für REF_GLOBAL könnte direkt nutzbar sein,
 *     WENN die Reihenfolge in der VariableTable mit der .global-Reihenfolge
 *     übereinstimmt. Dies muss verifiziert werden!
 *
 * TODO: [WICHTIG] Verschachtelte Funktionen korrekt behandeln
 *   Nach RenameNestedFunctions sind die Namen eindeutig (mit '*' Separator),
 *   aber die Funktionen sind NICHT flachgeklopft - sie sind immer noch
 *   in FunBody->FunDefs verschachtelt.
 *   Optionen:
 *   a) Alle Funktionen als global behandeln (einfach, erfordert ggf. isrg überall)
 *   b) Verschachtelungsebene korrekt tracken für isr/isrn/isrl
 *   c) Einen zusätzlichen Adjustment-Pass schreiben der Funktionen flachklopft
 *
 * TODO: [MITTEL] Label-Format standardisieren
 *   Labels sollten ein konsistentes Format haben, z.B.:
 *   - Funktionen: Funktionsname (nach RenameNested)
 *   - If-Labels: "if_else_%d", "if_end_%d"
 *   - Do-While-Labels: "do_loop_%d"
 *   - Für Jump/Branch: Labels statt numerische Offsets verwenden
 *
 * TODO: [MITTEL] emit()-Funktionen vollständig implementieren
 *   Die emit(), emit_label(), emit_pseudo() Hilfsfunktionen müssen
 *   mit variadischen Argumenten (va_list) implementiert werden.
 *
 *
 * TODO: [NIEDRIG] Ausgabedatei-Handling verbessern
 *   Aktuell über global.output_file. Könnte auch per Travdata
 *   oder als Parameter an die Phase übergeben werden.
 */
 
 /*
 make -C build-debug && ./test/codegen_tests/run_codegen_tests.bash
 */