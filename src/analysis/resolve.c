#include "ccn/ccn.h"
#include "palm/ctinfo.h"
#include "util/funtable.h"
#include <stdbool.h>
#include <stdlib.h>

void RSOinit(void) {}
void RSOfini(void) {}

node_st *RSOprogram(node_st *node) {
  struct data_rso *data = DATA_RSO_GET();
  data->functions = PROGRAM_FUNCTIONS(node);

  TRAVchildren(node);
  CTIabortOnError();

  return node;
}

node_st *RSOfundef(node_st *node) {
  struct data_rso *data = DATA_RSO_GET();
  FunctionTable *saved = data->functions;
  data->functions = FUNDEF_FUNCTIONS(node);

  TRAVchildren(node);

  data->functions = saved;

  return node;
}

node_st *RSOfuncall(node_st *node) {
  TRAVchildren(node);

  char *name = FUNCALL_NAME(node);

  struct data_rso *data = DATA_RSO_GET();
  bool contains = table_contains(data->functions, name);
  if (!contains) {
    CTI(CTI_ERROR, true, "cannot find function with name %s", name);
  }

  return node;
}
