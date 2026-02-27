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
}
void FTWfini(void) {}

/**
 * Traverses the program to set up the variable table context for the transformation.
 */
node_st *FTWprogram(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  data->variables = PROGRAM_VARIABLES(node);
  data->functions = PROGRAM_FUNCTIONS(node);
  TRAVchildren(node);
  return node;
}

/**
 * Updates the variable table context when entering a function.
 */
node_st *FTWfundef(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  VariableTable *old_v = data->variables;
  FunctionTable *old_f = data->functions;
  data->variables = FUNDEF_VARIABLES(node);
  data->functions = FUNDEF_FUNCTIONS(node);
  TRAVchildren(node);
  data->variables = old_v;
  data->functions = old_f;
  return node;
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

static node_st *create_while_loop(char *var_name, node_st *until, node_st *step, node_st *body_stmts, enum BinOpType op, struct data_ftw *data) {
  // condition: while (variable <=/ >= until)
  node_st *var_lhs_cond = ASTvar(NULL, STRcpy(var_name));
  VAR_VARPTR(var_lhs_cond) = return_varref_ignore_valid(data->variables, var_name);
  VAR_DIMENSIONEN(var_lhs_cond) = 0;
  node_st *var_ref_cond = ASTvarref(var_lhs_cond);
  EXPR_TYPE(var_ref_cond) = TY_int;

  node_st *cond = ASTbinop(var_ref_cond, until, op);
  EXPR_TYPE(cond) = TY_bool;

  // step: variable = variable + (step OR default 1)
  node_st *step_val = step ? step : ASTnum(1);
  if (NODE_TYPE(step_val) == NT_NUM) {
    EXPR_TYPE(step_val) = TY_int;
  }

  node_st *var_lhs_v = ASTvar(NULL, STRcpy(var_name));
  VAR_VARPTR(var_lhs_v) = return_varref_ignore_valid(data->variables, var_name);
  VAR_DIMENSIONEN(var_lhs_v) = 0;
  
  node_st *var_rhs_v = ASTvar(NULL, STRcpy(var_name));
  VAR_VARPTR(var_rhs_v) = return_varref_ignore_valid(data->variables, var_name);
  VAR_DIMENSIONEN(var_rhs_v) = 0;
  node_st *var_ref = ASTvarref(var_rhs_v);
  EXPR_TYPE(var_ref) = TY_int;
  
  node_st *add_expr = ASTbinop(var_ref, step_val, BO_add);
  EXPR_TYPE(add_expr) = TY_int;
  
  node_st *step_assign = ASTassign(var_lhs_v, add_expr);
  node_st *step_stmts = ASTstmts(step_assign, NULL);

  node_st *new_body = body_stmts;
  if (new_body == NULL) {
    new_body = step_stmts;
  } else {
    node_st *curr = new_body;
    while (STMTS_NEXT(curr) != NULL) {
      curr = STMTS_NEXT(curr);
    }
    STMTS_NEXT(curr) = step_stmts;
  }

  return ASTwhilestatement(ASTblock(new_body), cond);
}

/**
 * Handles the list of statements
 */
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
    
    // Transform
    STMTS_STMT(node) = TRAVdo(for_node);
    
    node_st *new_stmts = ASTstmts(assign, node);
    
    TRAVchildren(new_stmts);
    return new_stmts;
  }

  TRAVchildren(node);
  return node;
}

/**
 * Transforms for into while 
 */
node_st *FTWforstatement(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  if (data->free_mode) {
    TRAVchildren(node);
    return node;
  }

  TRAVchildren(node);
  // ... rest of the function ...

  char *var_name = FORSTATEMENT_VARIABLE(node);
  node_st *until = FORSTATEMENT_UNTIL(node);
  node_st *step = FORSTATEMENT_STEP(node);
  node_st *block = FORSTATEMENT_BLOCK(node);
  node_st *body_stmts = BLOCK_STMTS(block);

  node_st *res = NULL;

  // Take ownership of nodes to prevent them from being freed by CCNfree(node)
  FORSTATEMENT_UNTIL(node) = NULL;
  FORSTATEMENT_STEP(node) = NULL;
  BLOCK_STMTS(block) = NULL;

  if (is_constant(step)) {
    enum BinOpType op = is_negative(step) ? BO_ge : BO_le;
    res = create_while_loop(var_name, until, step, body_stmts, op, data);
  } else {
    // We must use one set of nodes and copy the other sets.
    node_st *step_copy = copy_and_fix(step, data);
    
    node_st *until_copy = copy_and_fix(until, data);
    node_st *step_copy2 = copy_and_fix(step, data);
    node_st *body_copy = copy_and_fix(body_stmts, data);
    
    node_st *while_pos = create_while_loop(var_name, until, step, body_stmts, BO_le, data);
    node_st *while_neg = create_while_loop(var_name, until_copy, step_copy2, body_copy, BO_ge, data);

    node_st *zero = ASTnum(0);
    EXPR_TYPE(zero) = TY_int;
    node_st *cond = ASTbinop(step_copy, zero, BO_ge);
    EXPR_TYPE(cond) = TY_bool;

    res = ASTifstatement(cond, ASTblock(ASTstmts(while_pos, NULL)), ASTblock(ASTstmts(while_neg, NULL)));
  }

  // Preserve source metrics
  NODE_BLINE(res) = NODE_BLINE(node);
  NODE_BCOL(res) = NODE_BCOL(node);
  NODE_ELINE(res) = NODE_ELINE(node);
  NODE_ECOL(res) = NODE_ECOL(node);

  // Before freeing the original node, free its attributes attributes uniquely owned by it
  // But wait, we took ownership of 'until', 'step', and 'body_stmts'.
  // We still need to free attributes of other potential children or the ones we didn't take.
  free_attributes(node, data);
  CCNfree(node);

  return res;
}



