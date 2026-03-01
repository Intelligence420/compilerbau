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
#include <string.h>

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
   VM-Spezifikation (2.2.1 ):
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
    data->arrexpr_slot_offset = 0;

    // Erstes Durchlauf - Funktions-Indizes für Imports
    int fun_import_counter = 0;
    node_st *decls_ptr = PROGRAM_DECLS(node);
    while (decls_ptr != NULL) {
        node_st *decl = DECLS_DECL(decls_ptr);
        if (NODE_TYPE(decl) == NT_FUNDEC) {
            char *name = FUNHEADER_NAME(FUNDEC_HEADER(decl));
            Function *f = funtable_get_function_ptr(data->functions, name);
            if (f) f->assembly_index = fun_import_counter++;
        }
        decls_ptr = DECLS_NEXT(decls_ptr);
    }

    // Variablen-Indizes (Imports und Globale)
    int var_import_counter = 0;
    int global_counter = 0;
    for (int i = 0; i < data->variables->size; i++) {
        Variable *v = &data->variables->variables[i];
        if (v->isextern) {
            v->assembly_index = var_import_counter++;
        } else {
            v->assembly_index = global_counter++;
        }
    }

    // Konstanten für Typecasts (0 und 0.0)
    Constant c0 = {.type = TY_int, .cint = 0};
    data->cint_0_idx = consttable_insert(data->constants, c0);
    Constant cf0 = {.type = TY_float, .cflt = 0.0};
    data->cflt_0_idx = consttable_insert(data->constants, cf0);

    // Synthetic __init for global variables if not defined in source
    // DeclarationSplitting creates a user __init fundef (not in function table),
    // so we check the decl list directly.
    bool has_user_init = false;
    {
        node_st *scan = PROGRAM_DECLS(node);
        while (scan != NULL) {
            node_st *d = DECLS_DECL(scan);
            if (NODE_TYPE(d) == NT_FUNDEF) {
                char *n = FUNHEADER_NAME(FUNDEF_HEADER(d));
                if (n && strcmp(n, "__init") == 0) {
                    has_user_init = true;
                    break;
                }
            }
            scan = DECLS_NEXT(scan);
        }
    }
    if (!has_user_init) {
        fprintf(outfile, "\n__init:\n");
        emit("esr 0");
        // Traverse global VarDefs to initialize arrays
        node_st *curr_decl = PROGRAM_DECLS(node);
        while (curr_decl != NULL) {
            node_st *decl = DECLS_DECL(curr_decl);
            if (NODE_TYPE(decl) == NT_VARDEF) {
                TRAVdo(decl);
            }
            curr_decl = DECLS_NEXT(curr_decl);
        }
        emit("return");
    }

    // Traverse functions (top-level labels)
    node_st *curr_decl = PROGRAM_DECLS(node);
    while (curr_decl != NULL) {
        node_st *decl = DECLS_DECL(curr_decl);
        if (NODE_TYPE(decl) == NT_FUNDEF) {
            TRAVdo(decl);
        }
        curr_decl = DECLS_NEXT(curr_decl);
    }

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
    for (int i = 0; i < data->variables->size; i++) {
        Variable *v = &data->variables->variables[i];
        if (v->isextern) {
            if (v->dim > 0) {
                emit_pseudo(".importvar \"%s\" %s[]", v->name, type_string(v->type));
            } else {
                emit_pseudo(".importvar \"%s\" %s", v->name, type_string(v->type));
            }
        }
    }

    // --- .global ---
    for (int i = 0; i < data->variables->size; i++) {
        Variable *v = &data->variables->variables[i];
        if (!v->isextern) {
            if (v->dim > 0) {
                emit_pseudo(".global %s[]", type_string(v->type));
            } else {
                emit_pseudo(".global %s", type_string(v->type));
            }
        }
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
    data->arrexpr_slot_offset = 0;

    // Count param slots
    data->param_slots = count_flat_params(FUNHEADER_PARAMS(header));

    // Alle lokalen Variablen und Parameter-Slots berechnen
    int current_slot = 0;
    for (int i = 0; i < data->variables->size; i++) {
        Variable *v = &data->variables->variables[i];
        // Check if a variable with the same name was already assigned a slot
        bool found_dup = false;
        for (int j = 0; j < i; j++) {
            if (STReq(data->variables->variables[j].name, v->name)) {
                v->slot_index = data->variables->variables[j].slot_index;
                found_dup = true;
                break;
            }
        }
        if (!found_dup) {
            v->slot_index = current_slot++;
        }
    }
    data->total_slots = current_slot;
    data->total_slots += 20; // Reserve 20 slots for temp array literal refs

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
     with an external definition of a variable" */
node_st *CGNglobaldec(node_st *node) {
    (void)node;
    return node;
}

/* CGNfunbody 
   VM-Spezifikation (esr, Abschnitt 1.4.4):
    "esr L — Enter subroutine. Advances the top of the stack with L elements,
     thus reserving space for L local variables."
    "esr should be issued at the beginning of a subroutine, thus ensuring that
     the locals and arguments are contiguous in the stack"
    "the first argument is addressable as local 0, and so on." */
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
    struct data_cgn *data = DATA_CGN_GET();
    Variable *v = vartable_get_variable_ptr(data->variables, VARDEF_NAME(node));
    if (v == NULL) return node;

    // Lokale Variable: Arrays allokieren
    node_st *dim_expr_list = VARDEF_EXPRS(node);
    if (dim_expr_list != NULL) {
        // Dimensionen evaluieren und speichern
        int i = 0;
        node_st *curr = dim_expr_list;
        while (curr != NULL) {
            TRAVdo(EXPRS_EXPR(curr));
            if (VARDEF_GLOBAL(node)) {
                emit("istoreg %d", v->assembly_index + 1 + i);
                emit("iloadg %d", v->assembly_index + 1 + i); // Reload for inewa
            } else {
                emit("istore %d", v->slot_index + 1 + i);
                emit("iload %d", v->slot_index + 1 + i); // Reload for inewa
            }
            curr = EXPRS_NEXT(curr);
            i++;
        }

        // Inewa
        if (i > 1) {
            for (int j = 1; j < i; j++) {
                emit("imul");
            }
        }
        emit("%cnewa", type_prefix(v->type));
        
        // Speichern
        if (VARDEF_GLOBAL(node)) {
            emit("astoreg %d", v->assembly_index);
        } else {
            emit("astore %d", v->slot_index);
        }
    }

    // Initializer handling (nur Skalar für jetzt)
    if (VARDEF_EXPR(node) != NULL) {
        TRAVdo(VARDEF_EXPR(node));
        if (VARDEF_GLOBAL(node)) {
            emit("%cstoreg %d", type_prefix(v->type), v->assembly_index);
        } else {
            emit("%cstore %d", type_prefix(v->type), v->slot_index);
        }
    }

    return node;
}

/* Forward declaration */
static int resolve_idx(struct data_cgn *data, VarReferenz *ref);

/* Statements */
node_st *CGNassign(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();

    node_st *var = ASSIGN_LET(node);
    if (var == NULL) return node;

    VarReferenz *ref = VAR_VARPTR(var);
    if (ref == NULL) return node;

    int idx = resolve_idx(data, ref);
    char prefix = type_prefix(ref->type);

    if (VAR_EXPRS(var) != NULL) {
        // Element access: a[i] = expr
        TRAVdo(ASSIGN_EXPR(node)); // RHS auf Stack
        TRAVdo(VAR_EXPRS(var)); // Index
        if (ref->reftype == REF_EXTERN) {
            emit("aloade %d", idx);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("aloadg %d", idx);
        } else {
            if (ref->n > 0) emit("aloadn %d %d", ref->n, idx);
            else emit("aload %d", idx);
        }
        emit("%cstorea", prefix);
    } else if (ref->dim > 0 && NODE_TYPE(ASSIGN_EXPR(node)) != NT_ARREXPR) {
        // Scalar-to-array broadcast: b = 7 where b is int[2,3]
        // Generate a fill loop over all elements
        // Store scalar value in a temp slot
        TRAVdo(ASSIGN_EXPR(node)); // RHS (scalar) auf Stack
        int temp_val = data->total_slots - 20 + data->arrexpr_slot_offset;
        data->arrexpr_slot_offset++;
        int temp_idx_slot = data->total_slots - 20 + data->arrexpr_slot_offset;
        data->arrexpr_slot_offset++;
        emit("%cstore %d", prefix, temp_val);

        // Calculate total size = dim0 * dim1 * ...
        for (int d = 0; d < ref->dim; d++) {
            if (ref->reftype == REF_EXTERN) {
                emit("iloade %d", idx + 1 + d);
            } else if (ref->reftype == REF_GLOBAL) {
                emit("iloadg %d", idx + 1 + d);
            } else {
                if (ref->n > 0) emit("iloadn %d %d", ref->n, idx + 1 + d);
                else emit("iload %d", idx + 1 + d);
            }
            if (d > 0) emit("imul");
        }
        // Total size is on stack, store to temp
        int temp_size = data->total_slots - 20 + data->arrexpr_slot_offset;
        data->arrexpr_slot_offset++;
        emit("istore %d", temp_size);

        // Initialize loop counter i = 0
        Constant c0 = {.type = TY_int, .cint = 0};
        int c0_idx = consttable_insert(data->constants, c0);
        emit("iloadc %d", c0_idx);
        emit("istore %d", temp_idx_slot);

        // Loop
        int lbl_loop = new_label(data);
        int lbl_end = new_label(data);
        emit_label("fill_%d", lbl_loop);
        emit("iload %d", temp_idx_slot);
        emit("iload %d", temp_size);
        emit("ilt");
        emit("branch_f fill_end_%d", lbl_end);

        // Store: value, index, array_ref
        emit("%cload %d", prefix, temp_val);
        emit("iload %d", temp_idx_slot);
        if (ref->reftype == REF_EXTERN) {
            emit("aloade %d", idx);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("aloadg %d", idx);
        } else {
            if (ref->n > 0) emit("aloadn %d %d", ref->n, idx);
            else emit("aload %d", idx);
        }
        emit("%cstorea", prefix);

        // i++
        emit("iload %d", temp_idx_slot);
        Constant c1 = {.type = TY_int, .cint = 1};
        int c1_idx = consttable_insert(data->constants, c1);
        emit("iloadc %d", c1_idx);
        emit("iadd");
        emit("istore %d", temp_idx_slot);
        emit("jump fill_%d", lbl_loop);
        emit_label("fill_end_%d", lbl_end);
    } else {
        TRAVdo(ASSIGN_EXPR(node)); // RHS auf Stack
        char full_prefix = (ref->dim > 0) ? 'a' : prefix;
        if (ref->reftype == REF_EXTERN) {
            emit("%cstoree %d", full_prefix, idx);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("%cstoreg %d", full_prefix, idx);
        } else {
            if (ref->n > 0) emit("%cstoren %d %d", full_prefix, ref->n, idx);
            else emit("%cstore %d", full_prefix, idx);
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

node_st *CGNvarref(node_st *node) {
    TRAVdo(VARREF_VAR(node));
    return node;
}

// Resolve actual index for a VarReferenz
// For locals: look up slot_index from variable table (handles name deduplication)
// For globals/externs: use ref->l directly (it's the assembly_index)
static int resolve_idx(struct data_cgn *data, VarReferenz *ref) {
    if (ref->reftype == REF_LOCAL && ref->n == 0 && 
        data->variables != NULL && ref->l < data->variables->size) {
        return data->variables->variables[ref->l].slot_index;
    }
    return ref->l;
}

node_st *CGNvar(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    VarReferenz *ref = VAR_VARPTR(node);
    if (ref == NULL) return node;

    // Use resolve_idx to map table index to actual slot index
    int idx = resolve_idx(data, ref);

    char prefix = type_prefix(ref->type);
    bool is_array_access = (VAR_EXPRS(node) != NULL);

    if (is_array_access) {
        TRAVdo(VAR_EXPRS(node));
        if (ref->reftype == REF_EXTERN) {
            emit("aloade %d", idx);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("aloadg %d", idx);
        } else {
            if (ref->n > 0) emit("aloadn %d %d", ref->n, idx);
            else emit("aload %d", idx);
        }
        emit("%cloada", prefix);
    } else {
        char full_prefix = (ref->dim > 0) ? 'a' : prefix;
        if (ref->reftype == REF_EXTERN) {
            emit("%cloade %d", full_prefix, idx);
        } else if (ref->reftype == REF_GLOBAL) {
            emit("%cloadg %d", full_prefix, idx);
        } else {
            if (ref->n > 0) emit("%cloadn %d %d", full_prefix, ref->n, idx);
            else emit("%cload %d", full_prefix, idx);
        }
        // When an array variable is used whole (e.g., as function argument),
        // push dimensions too.
        if (ref->dim > 0) {
            for (int i = 0; i < ref->dim; i++) {
                if (ref->reftype == REF_EXTERN) emit("iloade %d", idx + 1 + i);
                else if (ref->reftype == REF_GLOBAL) emit("iloadg %d", idx + 1 + i);
                else if (ref->n > 0) emit("iloadn %d %d", ref->n, idx + 1 + i);
                else emit("iload %d", idx + 1 + i);
            }
        }
    }
    return node;
}

node_st *CGNbinop(node_st *node) {
    enum BinOpType op = BINOP_OP(node);
    enum DeclarationType type = EXPR_TYPE(BINOP_LEFT(node));
    char prefix = type_prefix(type);

    // Short-circuit evaluation for && and ||
    if (op == BO_and) {
        struct data_cgn *data = DATA_CGN_GET();
        int lbl_false = new_label(data);
        int lbl_end   = new_label(data);
        TRAVdo(BINOP_LEFT(node));
        emit("branch_f sc_false_%d", lbl_false);
        TRAVdo(BINOP_RIGHT(node));
        emit("jump sc_end_%d", lbl_end);
        emit_label("sc_false_%d", lbl_false);
        emit("bloadc_f");
        emit_label("sc_end_%d", lbl_end);
        return node;
    }
    if (op == BO_or) {
        struct data_cgn *data = DATA_CGN_GET();
        int lbl_true = new_label(data);
        int lbl_end  = new_label(data);
        TRAVdo(BINOP_LEFT(node));
        emit("branch_t sc_true_%d", lbl_true);
        TRAVdo(BINOP_RIGHT(node));
        emit("jump sc_end_%d", lbl_end);
        emit_label("sc_true_%d", lbl_true);
        emit("bloadc_t");
        emit_label("sc_end_%d", lbl_end);
        return node;
    }

    TRAVdo(BINOP_LEFT(node));
    TRAVdo(BINOP_RIGHT(node));

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
    struct data_cgn *data = DATA_CGN_GET();
    TRAVdo(TYPECAST_EXPR(node));
    enum DeclarationType to = TYPECAST_TYPE(node);
    enum DeclarationType from = EXPR_TYPE(TYPECAST_EXPR(node));

    if (from == to) return node;

    if (to == TY_int) {
        if (from == TY_float) {
            emit("f2i");
        } else if (from == TY_bool) {
            // bool -> int: use conditional branch (no direct cast instruction)
            int lbl_true = new_label(data);
            int lbl_end  = new_label(data);
            emit("branch_t bool2int_true_%d", lbl_true);
            emit("iloadc %d", data->cint_0_idx);
            emit("jump bool2int_end_%d", lbl_end);
            emit_label("bool2int_true_%d", lbl_true);
            Constant c1 = {.type = TY_int, .cint = 1};
            int one_idx = consttable_insert(data->constants, c1);
            emit("iloadc %d", one_idx);
            emit_label("bool2int_end_%d", lbl_end);
        }
    } else if (to == TY_float) {
        if (from == TY_int) emit("i2f");
    } else if (to == TY_bool) {
        if (from == TY_int) {
            emit("iloadc %d", data->cint_0_idx);
            emit("ine");
        } else if (from == TY_float) {
            emit("floadc %d", data->cflt_0_idx);
            emit("fne");
        }
    }
    return node;
}

node_st *CGNarrexpr(node_st *node) {
    struct data_cgn *data = DATA_CGN_GET();
    int count = 0;
    node_st *exprs_ptr = ARREXPR_EXPRS(node);
    while (exprs_ptr != NULL) {
        count++;
        exprs_ptr = EXPRS_NEXT(exprs_ptr);
    }
    
    // Allocate array
    Constant con = {.type = TY_int, .cint = count};
    int size_idx = consttable_insert(data->constants, con);
    emit("iloadc %d", size_idx);
    
    char prefix = type_prefix(EXPR_TYPE(node));
    emit("%cnewa", prefix);
    
    // Use a temp slot for initialization
    int temp_slot = data->total_slots - 20 + data->arrexpr_slot_offset;
    data->arrexpr_slot_offset++;
    
    emit("astore %d", temp_slot);
    
    // Initialize elements
    exprs_ptr = ARREXPR_EXPRS(node);
    for (int i = 0; i < count; i++) {
        node_st *expr = EXPRS_EXPR(exprs_ptr);
        
        // Target order for storea: RHS (value), Index, Reference
        TRAVdo(expr); // Value
        
        Constant icon = {.type = TY_int, .cint = i};
        int idx_idx = consttable_insert(data->constants, icon);
        emit("iloadc %d", idx_idx); // Index
        
        emit("aload %d", temp_slot); // Reference
        
        emit("%cstorea", prefix);
        
        exprs_ptr = EXPRS_NEXT(exprs_ptr);
    }
    
    // Leave reference on stack
    emit("aload %d", temp_slot);
    
    data->arrexpr_slot_offset--;
    return node;
}
