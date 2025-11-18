/**
 * @file
 *
 * This file contains the code for the Print traversal.
 * The traversal has the uid: PRT
 *
 *
 */

#include "ccn/ccn.h"
#include "ccngen/ast.h"
#include "ccngen/trav.h"
#include "palm/dbug.h"
#include <stdio.h>

/**
 * @fn PRTprogram
 */
node_st *PRTprogram(node_st *node)
{
    TRAVstmts(node);
    return node;
}

/**
 * @fn PRTstmts
 */
node_st *PRTstmts(node_st *node)
{
    TRAVstmt(node);
    TRAVnext(node);
    return node;
}

/**
 * @fn PRTassign
 */
node_st *PRTassign(node_st *node)
{

    if (ASSIGN_LET(node) != NULL) {
        TRAVlet(node);
        printf( " = ");
    }

    TRAVexpr(node);
    printf( ";\n");


    return node;
}

/**
 * @fn PRTbinop
 */
node_st *PRTbinop(node_st *node)
{
    char *tmp = NULL;
    printf( "(");

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

    printf( " %s ", tmp);

    TRAVright(node);

    printf( ")(%d:%d-%d)", NODE_BLINE(node), NODE_BCOL(node), NODE_ECOL(node));

    return node;
}

node_st *PRTdeclaration(node_st *node) {
  char *decltype;
  switch (DECLARATION_TYPE(node)) {
    case TY_int: decltype = "int"; break;
    case TY_float: decltype = "float"; break;
    case TY_bool: decltype = "bool"; break;
  }

  printf("%s %s = ", decltype, DECLARATION_NAME(node));
  TRAVchildren(node);
  printf(";\n");
  return node;
}

node_st *PRTvoiddeclaration(node_st *node){

  printf("void;\n");
  return node;
}

node_st *PRTifstatement(node_st *node){
  printf("if ");
  TRAVexpr(node);
  printf("\n");

  TRAVblock(node);

  return node;
}

node_st *PRTifelsestatement(node_st *node){
  printf("if ");
  TRAVexpr(node);
  printf("\n");

  TRAVifblock(node);

  printf("else");
  printf("\n");

  TRAVelseblock(node);

  return node;
}

node_st *PRTwhilestatement(node_st *node){
  printf("while \n");
  TRAVexpr(node);
  printf("\n");

  TRAVblock(node);

  return node;
}

node_st *PRTdostatement(node_st *node){
  printf("do\n");

  TRAVblock(node);

  printf("while ");
  TRAVexpr(node);
  printf(";\n");

  return node;
}

node_st *PRTblock(node_st *node) {
  printf("{\n");
  TRAVchildren(node);
  printf("}\n");

  return node;
}

/**
 * @fn PRTvarlet
 */
node_st *PRTvarlet(node_st *node)
{
    printf("%s(%d:%d)", VARLET_NAME(node), NODE_BLINE(node), NODE_BCOL(node));
    return node;
}

/**
 * @fn PRTvar
 */
node_st *PRTvar(node_st *node)
{
    printf( "%s", VAR_NAME(node));
    return node;
}

/**
 * @fn PRTnum
 */
node_st *PRTnum(node_st *node)
{
    printf("%d", NUM_VAL(node));
    return node;
}

/**
 * @fn PRTfloat
 */
node_st *PRTfloat(node_st *node)
{
    printf( "%f", FLOAT_VAL(node));
    return node;
}

/**
 * @fn PRTbool
 */
node_st *PRTbool(node_st *node)
{
    char *bool_str = BOOL_VAL(node) ? "true" : "false";
    printf("%s", bool_str);
    return node;
}
