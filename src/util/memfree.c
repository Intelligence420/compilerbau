#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "palm/memory.h"
#include "user_types.h"
#include "util/funtable.h"
#include "util/vartable.h"

node_st *FREEprogram(node_st *node) {
  TRAVchildren(node);
  funtable_free(PROGRAM_FUNCTIONS(node));
  vartable_free(PROGRAM_VARIABLES(node));
  return node;
}

node_st *FREEfundef(node_st *node) {
  TRAVchildren(node);
  funtable_free(FUNDEF_FUNCTIONS(node));
  vartable_free(FUNDEF_VARIABLES(node));
  return node;
}

node_st *FREEvar(node_st *node) {
  TRAVchildren(node);
  MEMfree(VAR_VARPTR(node));
  return node;
}

node_st *FREEfuncall(node_st *node) {
  TRAVchildren(node);
  MEMfree(FUNCALL_FUNPTR(node));
  return node;
}
