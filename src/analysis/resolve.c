#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav_data.h"
#include "palm/ctinfo.h"
#include "palm/str.h"
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

node_st *RSOvardef(node_st *node) {
  TRAVchildren(node);

  char *name = VARDEF_NAME(node);
  enum DeclarationType type = VARDEF_TYPE(node);

  struct data_rso *data = DATA_RSO_GET();
  Variable var = {.name = STRcpy(name), .type = type};
  vartable_insert(data->variables, var);

  return node;
}

node_st *RSOglobaldec(node_st *node) {
  //TODO: 
  /*
    extern int x[m, ...];
                 ^ jedes m soll vom type int sein
  */
  TRAVchildren(node);

  char *name = GLOBALDEC_NAME(node);
  enum DeclarationType type = GLOBALDEC_TYPE(node);

  struct data_rso *data = DATA_RSO_GET();
  Variable var = {.name = STRcpy(name), .type = type};
  vartable_insert(data->variables, var);

  return node;
}

node_st *RSOvar(node_st *node) {
  TRAVchildren(node);

  char *name = VAR_NAME(node);
  struct data_rso *data = DATA_RSO_GET();

  bool contains = vartable_contains(data->variables, name);
  if (!contains) {
    CTI(CTI_ERROR, true, "cannot find variable with name %s", name);
  }

  return node;
}

node_st *RSOparam(node_st *node){
  //TODO: 
  /*
    int x[m, ...];
          ^ jedes m soll vom type int sein
  */

  TRAVchildren(node);

  char *name = PARAM_NAME(node);
  enum DeclarationType type = PARAM_TYPE(node);

  struct data_rso *data = DATA_RSO_GET();
  Variable var = {.name = STRcpy(name), .type = type};
  vartable_insert(data->variables, var);

  return node;
}