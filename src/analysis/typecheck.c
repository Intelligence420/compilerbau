#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "user_types.h"
#include "util/vartable.h"
#include <stdbool.h>

void TYCinit(void) {}
void TYCfini(void) {}

// Nodeset Expr
node_st *TYCnum(node_st *node) {
  TRAVchildren(node);
  EXPR_TYPE(node) = TY_int;
  return node;
}

node_st *TYCfloat(node_st *node) {
  TRAVchildren(node);
  EXPR_TYPE(node) = TY_float;
  return node;
}

node_st *TYCbool(node_st *node) {
  TRAVchildren(node);
  EXPR_TYPE(node) = TY_bool;
  return node;
}

node_st *TYCbinop(node_st *node) {
  TRAVchildren(node);
  node_st *left = BINOP_LEFT(node);
  node_st *right = BINOP_RIGHT(node);

  enum DeclarationType left_type = EXPR_TYPE(left);
  enum DeclarationType right_type = EXPR_TYPE(right);
  enum BinOpType binop_type = BINOP_OP(node);

  if (left_type != right_type) {
    CTI(CTI_ERROR, true, "Type error in binary operation: mismatched types.");
    // irgendwie CTI types einsetzten
  }

  switch (binop_type) {
  case BO_add:
  case BO_mul:
    if (left_type == TY_bool) {
      EXPR_TYPE(node) = TY_bool;
      break;
    }
    // fallthrough
  case BO_sub:
  case BO_div:
  case BO_mod:
    if (left_type != TY_int && left_type != TY_float) {
      CTI(CTI_ERROR, true,
          "Type error in binary operation: expected numeric types.");
    }
    EXPR_TYPE(node) = left_type;
    break;
  case BO_lt:
  case BO_le:
  case BO_gt:
  case BO_ge:
    if (left_type != TY_int && left_type != TY_float) {
      CTI(CTI_ERROR, true,
          "Type error in binary operation: expected numeric types for "
          "comparison.");
    }
    EXPR_TYPE(node) = TY_bool;
    break;
  case BO_eq:
  case BO_ne:
    EXPR_TYPE(node) = TY_bool;
    break;
  case BO_and:
  case BO_or:
    if (left_type != TY_bool) {
      CTI(CTI_ERROR, true,
          "Type error in binary operation: expected boolean types for logical "
          "operation.");
    }
    EXPR_TYPE(node) = TY_bool;
    break;
  default:
    DBUG_ASSERT(false, "Unknown binary operator.");
  }

  return node;
}

node_st *TYCmonop(node_st *node) {
  TRAVchildren(node);
  node_st *expr = MONOP_EXPR(node);
  enum MonOpType mo_op_type = MONOP_OP(node);
  enum DeclarationType type = EXPR_TYPE(expr);

  if (mo_op_type == MO_not) {
    if (type != TY_bool) {
      CTI(CTI_ERROR, true,
          "Type error in unary operation: expected boolean type for logical "
          "negation.");
    }
  } else if (mo_op_type == MO_neg) {
    if (type != TY_int && type != TY_float) {
      CTI(CTI_ERROR, true,
          "Type error in unary operation: expected numeric type for negation.");
    }
    EXPR_TYPE(node) = type;
  }
  return node;
}

node_st *TYCfuncall(node_st *node) {
  TRAVchildren(node);
  FunctionPtr func = FUNCALL_FUNPTR(node);
  EXPR_TYPE(node) = func->return_type;
  return node;
}

node_st *TYCvarref(node_st *node) {
  TRAVchildren(node);
  node_st *var = VARREF_VAR(node);
  VariablePtr var_ptr = VAR_VARPTR(var);
  EXPR_TYPE(node) = var_ptr->type;
  return node;
}

node_st *TYCtypecast(node_st *node) {
  TRAVchildren(node);
  enum DeclarationType type = TYPECAST_TYPE(node);
  node_st *expr = TYPECAST_EXPR(node);
  enum DeclarationType dec_type = EXPR_TYPE(expr);

  if (type == TY_void) {
    CTI(CTI_ERROR, true, "casting to void is illegal");
  }
  if (dec_type == TY_void) {
    CTI(CTI_ERROR, true, "casting from void is illegal");
  }

  return node;
}

// OTHER THINGERS
node_st *TYCvardef(node_st *node) {
  TRAVchildren(node);

  // TOOD: check array index expressions
  node_st *expr = VARDEF_EXPR(node);
  enum DeclarationType type = VARDEF_TYPE(node);

  if (expr != NULL) {
    if (EXPR_TYPE(expr) != type) {
      CTI(CTI_ERROR, true,
          "Type error in variable definition: initialization type mismatch.");
    }
  }

  return node;
}

node_st *TYCassign(node_st *node) {
  TRAVchildren(node);

  node_st *expr = ASSIGN_EXPR(node);
  node_st *var = ASSIGN_LET(node);
  VariablePtr var_ptr = VAR_VARPTR(var);

  if (var_ptr != NULL) {
    if (var_ptr->type != EXPR_TYPE(expr)) {
      CTI(CTI_ERROR, true,
          "Type error in assignment: mismatch between variable and expression "
          "types.");
    }
  }

  return node;
}

node_st *TYCifstatement(node_st *node) {
  TRAVchildren(node);
  node_st *expr = IFELSESTATEMENT_EXPR(node);

  if (EXPR_TYPE(expr) != TY_bool) {
    CTI(CTI_ERROR, true, "If condition must be boolean");
  }

  return node;
}

node_st *TYCifelsestatement(node_st *node) {
  TRAVchildren(node);
  node_st *expr = IFELSESTATEMENT_EXPR(node);

  if (EXPR_TYPE(expr) != TY_bool) {
    CTI(CTI_ERROR, true, "If-else condition must be boolean");
  }

  return node;
}

node_st *TYCwhilestatement(node_st *node) {
  TRAVchildren(node);
  node_st *expr = WHILESTATEMENT_EXPR(node);

  if (EXPR_TYPE(expr) != TY_bool) {
    CTI(CTI_ERROR, true, "While condition must be boolean");
  }

  return node;
}

node_st *TYCdostatement(node_st *node) {
  TRAVchildren(node);
  node_st *expr = DOSTATEMENT_EXPR(node);

  if (EXPR_TYPE(expr) != TY_bool) {
    CTI(CTI_ERROR, true, "Do-While condition must be boolean");
  }

  return node;
}

node_st *TYCforstatement(node_st *node) {
  TRAVchildren(node);
  node_st *step = FORSTATEMENT_STEP(node);
  node_st *until = FORSTATEMENT_UNTIL(node);
  // char *var = FORSTATEMENT_VARIABLE(node);
  node_st *init = FORSTATEMENT_INIT(node);

  if (step != NULL && EXPR_TYPE(step) != TY_int) {
    CTI(CTI_ERROR, true, "For loop step must be integer");
  }

  if (EXPR_TYPE(until) != TY_int) {
    CTI(CTI_ERROR, true, "For loop until must be integer");
  }

  if (EXPR_TYPE(init) != TY_int) {
    CTI(CTI_ERROR, true, "For loop init must be integer");
  }

  return node;
}

node_st *TYCfundef(node_st *node) {
  enum DeclarationType prev = DATA_TYC_GET()->return_type;
  DATA_TYC_GET()->return_type = FUNHEADER_TYPE(FUNDEF_HEADER(node));

  TRAVchildren(node);

  DATA_TYC_GET()->return_type = prev;

  return node;
}

node_st *TYCreturnstatement(node_st *node) {
  TRAVchildren(node);

  node_st *expr = RETURNSTATEMENT_EXPR(node);
  enum DeclarationType return_type = DATA_TYC_GET()->return_type;

  if (return_type == TY_void) {
    if (expr != NULL) {
      CTI(CTI_ERROR, true, "cannot return expression from void function");
    }
  } else {
    // return type is not void
    if (expr == NULL) {
      CTI(CTI_ERROR, true, "cannot return void from non-void functon");
    } else if (return_type != EXPR_TYPE(expr)) {
      CTI(CTI_ERROR, true, "return type mismatch");
    }
  }

  return node;
}
