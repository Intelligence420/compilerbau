#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/vartable.h"
#include "util/funtable.h"

void WDWinit(void) {
  struct data_wdw *data = DATA_WDW_GET();
  data->fix_mode = false;
  data->variables = NULL;
  data->functions = NULL;
}
void WDWfini(void) {}

node_st *WDWprogram(node_st *node) {
  struct data_wdw *data = DATA_WDW_GET();
  data->variables = PROGRAM_VARIABLES(node);
  data->functions = PROGRAM_FUNCTIONS(node);
  TRAVchildren(node);
  return node;
}

node_st *WDWfundef(node_st *node) {
  struct data_wdw *data = DATA_WDW_GET();
  VariableTable *old_v = data->variables;
  FunctionTable *old_f = data->functions;
  data->variables = FUNDEF_VARIABLES(node);
  data->functions = FUNDEF_FUNCTIONS(node);
  TRAVchildren(node);
  data->variables = old_v;
  data->functions = old_f;
  return node;
}

node_st *WDWvar(node_st *node) {
  struct data_wdw *data = DATA_WDW_GET();
  if (data->fix_mode && data->variables != NULL) {
    VAR_VARPTR(node) = return_varref_ignore_valid(data->variables, VAR_NAME(node));
  }
  TRAVchildren(node);
  return node;
}

node_st *WDWfuncall(node_st *node) {
  struct data_wdw *data = DATA_WDW_GET();
  if (data->fix_mode && data->functions != NULL) {
    FUNCALL_FUNPTR(node) = funtable_get_function(data->functions, FUNCALL_NAME(node));
  }
  TRAVchildren(node);
  return node;
}

node_st *WDWwhilestatement(node_st *node) {
  TRAVchildren(node);

  node_st *expr = WHILESTATEMENT_EXPR(node);
  node_st *block = WHILESTATEMENT_BLOCK(node);

  WHILESTATEMENT_EXPR(node) = NULL;
  WHILESTATEMENT_BLOCK(node) = NULL;

  // if (expr) { do { block } while (copy(expr)); }
  node_st *expr_copy = CCNcopy(expr);
  
  struct data_wdw *data = DATA_WDW_GET();
  data->fix_mode = true;
  TRAVdo(expr_copy);
  data->fix_mode = false;
  
  node_st *do_while = ASTdostatement(block, expr);
  
  node_st *if_stmts = ASTstmts(do_while, NULL);
  node_st *if_stmt = ASTifstatement(expr_copy, ASTblock(if_stmts), NULL);

  NODE_BLINE(if_stmt) = NODE_BLINE(node);
  NODE_BCOL(if_stmt) = NODE_BCOL(node);
  NODE_ELINE(if_stmt) = NODE_ELINE(node);
  NODE_ECOL(if_stmt) = NODE_ECOL(node);

  CCNfree(node);

  return if_stmt;
}
