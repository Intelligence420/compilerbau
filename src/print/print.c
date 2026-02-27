/**
 * @file
 *
 * This file contains the code for the Print traversal.
 * The traversal has the uid: PRT
 *
 *
 */

#include "util/print.h"
#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "palm/dbug.h"
#include <stdio.h>

static int indent = 0;

static void print_indent() {
  for (int i = 0; i < indent; i++) {
    printf("  ");
  }
}

/**
 * @fn PRTprogram
 */
node_st *PRTprogram(node_st *node) {
  indent = 0;
  TRAVchildren(node);
  return node;
}

node_st *PRTdecls(node_st *node) {
  TRAVchildren(node);
  return node;
}

/**
 * @fn PRTstmts
 */
node_st *PRTstmts(node_st *node) {
  TRAVstmt(node);
  TRAVnext(node);
  return node;
}

/**
 * @fn PRTassign
 */
node_st *PRTassign(node_st *node) {

  print_indent();
  if (ASSIGN_LET(node) != NULL) {
    TRAVlet(node);
    printf(" = ");
  }

  TRAVexpr(node);
  printf(";\n");

  return node;
}

node_st *PRTfundef(node_st *node) {
  print_indent();
  if (FUNDEF_EXPORT(node)) {
    printf("export ");
  }
  TRAVheader(node);
  TRAVbody(node);

  return node;
}

node_st *PRTfundefs(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *PRTfunheader(node_st *node) {
  char *decl_type = TYstr(FUNHEADER_TYPE(node));

  printf("%s %s", decl_type, FUNHEADER_NAME(node));
  printf("(");
  TRAVparams(node);
  printf(") ");
  return node;
}

node_st *PRTfunbody(node_st *node) {
  printf("{\n");
  indent++;
  TRAVchildren(node);
  indent--;
  printf("}\n\n");
  return node;
}

node_st *PRTfuncallstmt(node_st *node) {

  print_indent();
  TRAVchildren(node);
  printf(";\n");

  return node;
}

node_st *PRTids(node_st *node) {

  TRAVchildren(node);

  return node;
}

node_st *PRTvardef(node_st *node) {
  print_indent();
  if (VARDEF_EXPORT(node)) {
    printf("export ");
  }

  printf("%s", TYstr(VARDEF_TYPE(node)));

  if (VARDEF_EXPRS(node) != NULL) {
    printf("[");
    TRAVexprs(node);
    printf("]");
  }

  printf(" %s", VARDEF_NAME(node));

  if (VARDEF_EXPR(node) != NULL) {
    printf(" = ");
    TRAVexpr(node);
  }

  printf(";\n");

  return node;
}

node_st *PRTvardefs(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *PRTarrexpr(node_st *node) {
  printf("[");
  TRAVchildren(node);
  printf("]");
  return node;
}

node_st *PRTlocalfundef(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *PRTglobaldec(node_st *node) {
  print_indent();
  printf("extern %s %s;\n", TYstr(GLOBALDEC_TYPE(node)), GLOBALDEC_NAME(node));
  TRAVchildren(node);
  return node;
}

node_st *PRTfundec(node_st *node) {
  print_indent();
  printf("extern ");
  TRAVchildren(node);
  printf(";\n");

  return node;
}

node_st *PRTvarref(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *PRTvardecs(node_st *node) {
  TRAVchildren(node);
  return node;
}

node_st *PRTparams(node_st *node) {
  TRAVparam(node);

  /* Nur Komma drucken und rekursiv weitermachen, wenn noch ein weiteres
   * Param-Node existiert */
  if (PARAMS_PARAMS(node) != NULL) {
    printf(", ");
    TRAVparams(node);
  }

  return node;
}

node_st *PRTparam(node_st *node) {
  TRAVchildren(node);
  char *param_type = TYstr(PARAM_TYPE(node));

  printf("%s %s", param_type, PARAM_NAME(node));
  return node;
}

/**
 * @fn PRTbinop
 */
node_st *PRTbinop(node_st *node) {
  char *tmp = NULL;

  TRAVleft(node);

  switch (BINOP_OP(node)) {
  case BO_add:
    tmp = "+";
    break;
  case BO_sub:
    tmp = "-";
    break;
  case BO_mul:
    tmp = "*";
    break;
  case BO_div:
    tmp = "/";
    break;
  case BO_mod:
    tmp = "%";
    break;
  case BO_lt:
    tmp = "<";
    break;
  case BO_le:
    tmp = "<=";
    break;
  case BO_gt:
    tmp = ">";
    break;
  case BO_ge:
    tmp = ">=";
    break;
  case BO_eq:
    tmp = "==";
    break;
  case BO_ne:
    tmp = "!=";
    break;
  case BO_or:
    tmp = "||";
    break;
  case BO_and:
    tmp = "&&";
    break;
  case BO_NULL:
    DBUG_ASSERT(false, "unknown binop detected!");
  }

  printf(" %s ", tmp);

  TRAVright(node);

  return node;
}

node_st *PRTmonop(node_st *node) {
  char *tmp = NULL;
  printf("(");

  switch (MONOP_OP(node)) {
  case MO_not:
    tmp = "!";
    break;
  case MO_neg:
    tmp = "-";
    break;
  case MO_NULL:
    DBUG_ASSERT(false, "unknown monop detected!");
  }

  printf("%s", tmp);

  TRAVexpr(node);

  printf(")");

  return node;
}

node_st *PRTfuncall(node_st *node) {
  printf("%s(", FUNCALL_NAME(node));
  TRAVexprs(node);
  printf(")");

  return node;
}

node_st *PRTexprs(node_st *node) {
  TRAVexpr(node);
  for (node_st *next = EXPRS_NEXT(node); next != NULL;
       next = EXPRS_NEXT(next)) {
    printf(", ");
    TRAVexpr(next);
  }

  return node;
}

node_st *PRTtypecast(node_st *node) {
  char *ty = TYstr(TYPECAST_TYPE(node));

  printf("(%s) ", ty);
  TRAVexpr(node);

  return node;
}

node_st *PRTvardec(node_st *node) {
  print_indent();
  char *decltype = TYstr(VARDEF_TYPE(node));

  printf("%s %s", decltype, VARDEF_NAME(node));
  if (VARDEF_EXPR(node) != NULL) {
    printf(" = ");
  }
  TRAVchildren(node);
  printf(";\n");
  return node;
}

node_st *PRTifstatement(node_st *node) {
  print_indent();
  printf("if (");
  TRAVexpr(node);
  printf(")\n");

  TRAVblock(node);

  if (IFSTATEMENT_ELSEBLOCK(node) != NULL) {
    print_indent();
    printf("else\n");
    TRAVelseblock(node);
  }

  return node;
}

node_st *PRTwhilestatement(node_st *node) {
  print_indent();
  printf("while (");
  TRAVexpr(node);
  printf(")\n");

  TRAVblock(node);

  return node;
}

node_st *PRTdostatement(node_st *node) {
  print_indent();
  printf("do\n");

  TRAVblock(node);

  print_indent();
  printf("while (");
  TRAVexpr(node);
  printf(");\n");

  return node;
}

node_st *PRTforstatement(node_st *node) {
  print_indent();
  printf("for (int %s = ", FORSTATEMENT_VARIABLE(node));
  TRAVinit(node);
  printf(", ");

  TRAVuntil(node);
  if (FORSTATEMENT_STEP(node) != NULL) {
    printf(", ");
    TRAVstep(node);
  }
  printf(")\n");

  TRAVblock(node);

  return node;
}

node_st *PRTreturnstatement(node_st *node) {
  print_indent();
  printf("return ");
  TRAVexpr(node);
  printf(";\n");
  return node;
}

node_st *PRTblock(node_st *node) {
  print_indent();
  printf("{\n");
  indent++;
  TRAVchildren(node);
  indent--;
  print_indent();
  printf("}\n");

  return node;
}

/**
 * @fn PRTvarlet
 */
node_st *PRTvar(node_st *node) {
  printf("%s", VAR_NAME(node));
  if (VAR_EXPRS(node) != NULL) {
    printf("[");
    TRAVexprs(node);
    printf("]");
  }
  return node;
}

/**
 * @fn PRTnum
 */
node_st *PRTnum(node_st *node) {
  printf("%d", NUM_VAL(node));
  return node;
}

/**
 * @fn PRTfloat
 */
node_st *PRTfloat(node_st *node) {
  printf("%f", FLOAT_VAL(node));
  return node;
}

/**
 * @fn PRTbool
 */
node_st *PRTbool(node_st *node) {
  char *bool_str = BOOL_VAL(node) ? "true" : "false";
  printf("%s", bool_str);
  return node;
}
