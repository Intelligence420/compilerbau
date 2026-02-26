#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/vartable.h"

void FTWinit(void) {}
void FTWfini(void) {}

/**
 * Traverses the program to set up the variable table context for the transformation.
 */
node_st *FTWprogram(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  data->variables = PROGRAM_VARIABLES(node);
  TRAVchildren(node);
  return node;
}

/**
 * Updates the variable table context when entering a function.
 */
node_st *FTWfundef(node_st *node) {
  struct data_ftw *data = DATA_FTW_GET();
  VariableTable *old = data->variables;
  data->variables = FUNDEF_VARIABLES(node);
  TRAVchildren(node);
  data->variables = old;
  return node;
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

/**
 * Handles the list of statements
 */
node_st *FTWstmts(node_st *node) {
  if (NODE_TYPE(STMTS_STMT(node)) == NT_FORSTATEMENT) {
    node_st *for_node = STMTS_STMT(node);
    char *var_name = FORSTATEMENT_VARIABLE(for_node);
    node_st *init_expr = FORSTATEMENT_INIT(for_node);

    struct data_ftw *data = DATA_FTW_GET();
    
    // Create variable = init_expr assignment
    node_st *var_node = ASTvar(NULL, STRcpy(var_name));
    VAR_VARPTR(var_node) = return_varref_ignore_valid(data->variables, var_name);
    VAR_DIMENSIONEN(var_node) = 0;
    
    node_st *assign = ASTassign(var_node, CCNcopy(init_expr));
    
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
  TRAVchildren(node);

  char *var_name = FORSTATEMENT_VARIABLE(node);
  node_st *until = FORSTATEMENT_UNTIL(node);
  node_st *step = FORSTATEMENT_STEP(node);
  node_st *block = FORSTATEMENT_BLOCK(node);

  struct data_ftw *data = DATA_FTW_GET();

  // condition?: while (variable <=/ >= until)
  node_st *var_lhs_cond = ASTvar(NULL, STRcpy(var_name));
  VAR_VARPTR(var_lhs_cond) = return_varref_ignore_valid(data->variables, var_name);
  VAR_DIMENSIONEN(var_lhs_cond) = 0;
  node_st *var_ref_cond = ASTvarref(var_lhs_cond);
  EXPR_TYPE(var_ref_cond) = TY_int;

  enum BinOpType op = is_negative(step) ? BO_ge : BO_le;
  node_st *cond = ASTbinop(var_ref_cond, CCNcopy(until), op);
  EXPR_TYPE(cond) = TY_bool;

  // step?: variable = variable + (step ODER default welcher eins ist)
  node_st *step_val = step ? CCNcopy(step) : ASTnum(1);
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

  // füge step am ende des block
  if (BLOCK_STMTS(block) == NULL) {
    BLOCK_STMTS(block) = step_stmts;
  } else {
    node_st *curr = BLOCK_STMTS(block);
    while (STMTS_NEXT(curr) != NULL) {
      curr = STMTS_NEXT(curr);
    }
    STMTS_NEXT(curr) = step_stmts;
  }

  // while
  node_st *body_stmts = BLOCK_STMTS(block);
  BLOCK_STMTS(block) = NULL; // Take ownership from FOR block
  
  node_st *while_node = ASTwhilestatement(ASTblock(body_stmts), cond);

  // Preserve source metrics
  NODE_BLINE(while_node) = NODE_BLINE(node);
  NODE_BCOL(while_node) = NODE_BCOL(node);
  NODE_ELINE(while_node) = NODE_ELINE(node);
  NODE_ECOL(while_node) = NODE_ECOL(node);

  CCNfree(node);

  return while_node;
}
