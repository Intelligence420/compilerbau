#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/vartable.h"

#include "util/funtable.h"

void FTWinit(void) {
  struct data_ftw *data = DATA_FTW_GET();
  data->variables = NULL;
  data->functions = NULL;
  data->fix_mode = false;
  data->free_mode = false;
  data->current_fundef = NULL;
}
void FTWfini(void) {}

/* Traverses the program to set up the variable table context for the transformation */
node_st *FTWprogram(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  data->variables = PROGRAM_VARIABLES(node);
  data->functions = PROGRAM_FUNCTIONS(node);
  TRAVchildren(node);
  return node;
}

/* Updates the variable table context when entering a function */
node_st *FTWfundef(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  VariableTable *old_v = data->variables;
  FunctionTable *old_f = data->functions;
  node_st *old_fundef = data->current_fundef;
  data->variables = FUNDEF_VARIABLES(node);
  data->functions = FUNDEF_FUNCTIONS(node);
  data->current_fundef = node;
  TRAVchildren(node);
  data->variables = old_v;
  data->functions = old_f;
  data->current_fundef = old_fundef;
  return node;
}

static void ensure_vardef(struct data_ftw *data, char *name) {
  if (data->current_fundef == NULL) return;
  node_st *body = FUNDEF_BODY(data->current_fundef);
  node_st *decls = FUNBODY_DECL(body);
  while (decls != NULL) {
    if (STReq(VARDEF_NAME(VARDEFS_DEC(decls)), name)) {
      return; // Already exists
    }
    decls = VARDEFS_NEXT(decls);
  }
  // Check params
  node_st *header = FUNDEF_HEADER(data->current_fundef);
  node_st *params = FUNHEADER_PARAMS(header);
  while (params != NULL) {
    if (STReq(PARAM_NAME(PARAMS_PARAM(params)), name)) {
      return; 
    }
    params = PARAMS_PARAMS(params);
  }

  // Create new VarDef
  node_st *new_vardef = ASTvardef(NULL, NULL, TY_int, STRcpy(name));
  FUNBODY_DECL(body) = ASTvardefs(new_vardef, FUNBODY_DECL(body));

  // Re-validate the for-loop variable in the variable table
  // find the first entry with this name and make it valid/writable
  if (data->variables != NULL) {
    for (int i = 0; i < data->variables->size; i++) {
      if (STReq(data->variables->variables[i].name, name)) {
        data->variables->variables[i].valid = true;
        data->variables->variables[i].readonly = false;
        break;
      }
    }
  }
}

node_st *FTWvar(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  if (data->free_mode) {
    if (VAR_VARPTR(node) != NULL) {
      MEMfree(VAR_VARPTR(node));
      VAR_VARPTR(node) = NULL;
    }
  } else if (data->fix_mode && data->variables != NULL) {
    VAR_VARPTR(node) = return_varref_ignore_valid(data->variables, VAR_NAME(node));
  }
  TRAVchildren(node);
  return node;
}

node_st *FTWfuncall(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  if (data->free_mode) {
    if (FUNCALL_FUNPTR(node) != NULL) {
      MEMfree(FUNCALL_FUNPTR(node));
      FUNCALL_FUNPTR(node) = NULL;
    }
  } else if (data->fix_mode && data->functions != NULL) {
    FUNCALL_FUNPTR(node) = funtable_get_function(data->functions, FUNCALL_NAME(node));
  }
  TRAVchildren(node);
  return node;
}

// Hilfs Funktionen
static void free_attributes(node_st *node, struct data_ftw *data);

static node_st *copy_and_fix(node_st *node, struct data_ftw *data) {
  if (node == NULL) return NULL;
  node_st *copy = CCNcopy(node);
  bool old_fix = data->fix_mode;
  data->fix_mode = true;
  TRAVdo(copy);
  data->fix_mode = old_fix;
  return copy;
}

static void free_attributes(node_st *node, struct data_ftw *data) {
  if (node == NULL) return;
  bool old_free = data->free_mode;
  data->free_mode = true;
  TRAVdo(node);
  data->free_mode = old_free;
}

static bool is_constant(node_st *node) {
  if (node == NULL) return true;
  if (NODE_TYPE(node) == NT_NUM) return true;
  if (NODE_TYPE(node) == NT_MONOP && MONOP_OP(node) == MO_neg) {
    node_st *child = MONOP_EXPR(node);
    return NODE_TYPE(child) == NT_NUM;
  }
  return false;
}

static bool is_negative(node_st *node) {
  if (node == NULL) return false;
  if (NODE_TYPE(node) == NT_NUM) return NUM_VAL(node) < 0;
  if (NODE_TYPE(node) == NT_MONOP && MONOP_OP(node) == MO_neg) {
    node_st *child = MONOP_EXPR(node);
    if (NODE_TYPE(child) == NT_NUM) return NUM_VAL(child) > 0;
  }
  return false;
}

/* Handles the list of statements */
node_st *FTWstmts(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  if (data->free_mode) {
    TRAVchildren(node);
    return node;
  }
  
  if (NODE_TYPE(STMTS_STMT(node)) == NT_FORSTATEMENT) {
    node_st *for_node = STMTS_STMT(node);
    char *var_name = FORSTATEMENT_VARIABLE(for_node);
    node_st *init_expr = FORSTATEMENT_INIT(for_node);

    // Create variable = init_expr assignment
    node_st *var_node = ASTvar(NULL, STRcpy(var_name));
    VAR_VARPTR(var_node) = return_varref_ignore_valid(data->variables, var_name);
    VAR_DIMENSIONEN(var_node) = 0;
    
    // Take ownership of init_expr
    FORSTATEMENT_INIT(for_node) = NULL;
    node_st *assign = ASTassign(var_node, init_expr);
    
    // Ensure declaration exists
    ensure_vardef(data, var_name);

    // Transform
    STMTS_STMT(node) = TRAVdo(for_node);
    
    node_st *new_stmts = ASTstmts(assign, node);
    
    TRAVchildren(new_stmts);
    return new_stmts;
  }

  TRAVchildren(node);
  return node;
}

/* Transforms for into while */
node_st *FTWforstatement(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  if (data->free_mode) {
    TRAVchildren(node);
    return node;
  }

  TRAVchildren(node);

  char *var_name = FORSTATEMENT_VARIABLE(node);
  node_st *until = FORSTATEMENT_UNTIL(node);
  node_st *orig_step = FORSTATEMENT_STEP(node);
  node_st *step = orig_step ? orig_step : ASTnum(1);
  if (NODE_TYPE(step) == NT_NUM) EXPR_TYPE(step) = TY_int;
  node_st *block = FORSTATEMENT_BLOCK(node);
  node_st *body_stmts = BLOCK_STMTS(block);

  // Take ownership of nodes
  FORSTATEMENT_UNTIL(node) = NULL;
  FORSTATEMENT_STEP(node) = NULL;
  BLOCK_STMTS(block) = NULL;

  // step assignment: var = var + step
  node_st *var_lhs_s = ASTvar(NULL, STRcpy(var_name));
  VAR_VARPTR(var_lhs_s) = return_varref_ignore_valid(data->variables, var_name);
  VAR_DIMENSIONEN(var_lhs_s) = 0;
  
  node_st *var_rhs_s = ASTvar(NULL, STRcpy(var_name));
  VAR_VARPTR(var_rhs_s) = return_varref_ignore_valid(data->variables, var_name);
  VAR_DIMENSIONEN(var_rhs_s) = 0;
  node_st *var_ref_s = ASTvarref(var_rhs_s);
  EXPR_TYPE(var_ref_s) = TY_int;
  
  node_st *step_copy_assign = copy_and_fix(step, data);
  node_st *add = ASTbinop(var_ref_s, step_copy_assign, BO_add);
  EXPR_TYPE(add) = TY_int;
  node_st *assign = ASTassign(var_lhs_s, add);
  node_st *new_stmts = body_stmts;
  if (new_stmts == NULL) {
    new_stmts = ASTstmts(assign, NULL);
  } else {
    node_st *curr = new_stmts;
    while (STMTS_NEXT(curr)) curr = STMTS_NEXT(curr);
    STMTS_NEXT(curr) = ASTstmts(assign, NULL);
  }

  node_st *res = NULL;

  if (is_constant(step)) {
    enum BinOpType op = is_negative(step) ? BO_gt : BO_lt;
    
    node_st *var_lhs_cond = ASTvar(NULL, STRcpy(var_name));
    VAR_VARPTR(var_lhs_cond) = return_varref_ignore_valid(data->variables, var_name);
    VAR_DIMENSIONEN(var_lhs_cond) = 0;
    node_st *var_ref_cond = ASTvarref(var_lhs_cond);
    EXPR_TYPE(var_ref_cond) = TY_int;

    node_st *until_copy = copy_and_fix(until, data);
    node_st *cond = ASTbinop(var_ref_cond, until_copy, op);
    EXPR_TYPE(cond) = TY_bool;

    res = ASTwhilestatement(ASTblock(new_stmts), cond);

  } else {
    // General structure: if (step >= 0) { while(i <= until) } else { while(i >= until) }
    node_st *var_lhs_c1 = ASTvar(NULL, STRcpy(var_name));
    VAR_VARPTR(var_lhs_c1) = return_varref_ignore_valid(data->variables, var_name);
    VAR_DIMENSIONEN(var_lhs_c1) = 0;
    node_st *var_ref_c1 = ASTvarref(var_lhs_c1);
    EXPR_TYPE(var_ref_c1) = TY_int;
    
    node_st *until_copy1 = copy_and_fix(until, data);
    node_st *cond_pos = ASTbinop(var_ref_c1, until_copy1, BO_lt);
    EXPR_TYPE(cond_pos) = TY_bool;

    node_st *var_lhs_c2 = ASTvar(NULL, STRcpy(var_name));
    VAR_VARPTR(var_lhs_c2) = return_varref_ignore_valid(data->variables, var_name);
    VAR_DIMENSIONEN(var_lhs_c2) = 0;
    node_st *var_ref_c2 = ASTvarref(var_lhs_c2);
    EXPR_TYPE(var_ref_c2) = TY_int;

    node_st *until_copy2 = copy_and_fix(until, data);
    node_st *cond_neg = ASTbinop(var_ref_c2, until_copy2, BO_gt);
    EXPR_TYPE(cond_neg) = TY_bool;

    node_st *step_copy1 = copy_and_fix(step, data);
    node_st *body_copy = copy_and_fix(new_stmts, data);
    
    node_st *while_pos = ASTwhilestatement(ASTblock(new_stmts), cond_pos);
    node_st *while_neg = ASTwhilestatement(ASTblock(body_copy), cond_neg);

    node_st *zero = ASTnum(0);
    EXPR_TYPE(zero) = TY_int;
    node_st *cond_dir = ASTbinop(step_copy1, zero, BO_ge);
    EXPR_TYPE(cond_dir) = TY_bool;

    res = ASTifstatement(cond_dir, ASTblock(ASTstmts(while_pos, NULL)), ASTblock(ASTstmts(while_neg, NULL)));
  }

  NODE_BLINE(res) = NODE_BLINE(node);
  NODE_BCOL(res) = NODE_BCOL(node);
  NODE_ELINE(res) = NODE_ELINE(node);
  NODE_ECOL(res) = NODE_ECOL(node);

  free_attributes(node, data);
  free_attributes(until, data);
  if (orig_step) {
    free_attributes(orig_step, data);
  } else {
    free_attributes(step, data);
  }
  CCNfree(node);
  CCNfree(until);
  if (orig_step == NULL) {
    CCNfree(step);
  } else {
    CCNfree(orig_step);
  }

  return res;
}



