#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "palm/ctinfo.h"
#include "util/funtable.h"
#include "util/vartable.h"
#include <stdbool.h>
#include <stdlib.h>

void RSOinit(void) {}
void RSOfini(void) {}

node_st *RSOprogram(node_st *node) {
  struct data_rso *data = DATA_RSO_GET();
  data->functions = PROGRAM_FUNCTIONS(node);
  data->variables = create_vartable(NULL);

  TRAVchildren(node);
  CTIabortOnError();

  PROGRAM_VARIABLES(node) = data->variables;

  return node;
}

node_st *RSOfundef(node_st *node) {
  struct data_rso *data = DATA_RSO_GET();
  FunctionTable *functions_safe = data->functions;
  data->functions = FUNDEF_FUNCTIONS(node);
  VariableTable *variables_safe = data->variables;
  data->variables = create_vartable(data->variables);

  TRAVchildren(node);

  FUNDEF_VARIABLES(node) = data->variables;

  data->functions = functions_safe;
  data->variables = variables_safe;

  return node;
}

node_st *RSOfuncall(node_st *node) {
  TRAVchildren(node);

  char *name = FUNCALL_NAME(node);

  struct data_rso *data = DATA_RSO_GET();
  bool contains = funtable_contains(data->functions, name);
  if (!contains) {
    CTI(CTI_ERROR, true, "cannot find function with name %s", name);
  }

  return node;
}
