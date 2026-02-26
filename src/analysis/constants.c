#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "util/consttable.h"
#include <stdbool.h>

void CSTinit(void) {}
void CSTfini(void) {}

node_st *CSTprogram(node_st *node) {
  struct data_cst *data = DATA_CST_GET();
  data->constants = create_consttable(); // globale Ebene

  TRAVchildren(node);

  PROGRAM_CONSTANTS(node) = data->constants;

  return node;
}

node_st *CSTnum(node_st *node) {
  TRAVchildren(node);
  struct data_cst *data = DATA_CST_GET();
  Constant consti = {.type = TY_int, .cint = NUM_VAL(node)};
  int idx = consttable_insert(data->constants, consti);
  NUM_G(node) = idx;
  return node;
}

node_st *CSTfloat(node_st *node) {
  TRAVchildren(node);
  struct data_cst *data = DATA_CST_GET();
  Constant consti = {.type = TY_float, .cflt = FLOAT_VAL(node)};
  int idx = consttable_insert(data->constants, consti);
  FLOAT_G(node) = idx;

  return node;
}
