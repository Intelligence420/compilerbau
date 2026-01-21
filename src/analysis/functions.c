/**
 * Was ich mir so alle von Mai gemerkt habe (basically nichts, aber: Wer will
 * mich aufhalten?):
 * - Jede Scope-Ebene hat eine eigene Funktionsliste
 * - Tiefere Ebenen haben einen Parent-Pointer zur übergeordneten Liste
 * - Bei voller Kapazität wird die Liste automatisch per realloc verdoppelt
 */

#include "ccn/ccn.h"
#include "ccn/dynamic_core.h"
#include "ccngen/ast.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ccngen/trav_data.h"
#include "palm/memory.h"
#include "palm/str.h"
#include "util/funtable.h"

void FNSinit(void) {}
void FNSfini(void) {}

static Function from_header(node_st *node, bool isextern) {
  const char *func_name = FUNHEADER_NAME(node);
  enum DeclarationType type = FUNHEADER_TYPE(node);

  int arity = 0;
  node_st *param = FUNHEADER_PARAMS(node);
  while (param != NULL) {
    arity++;
    param = PARAMS_PARAMS(param);
  }

  Parameter *list = MEMmalloc(arity * sizeof(Parameter));
  param = FUNHEADER_PARAMS(node);
  int i = 0;
  while (param != NULL) {
    node_st *ids = PARAM_IDS(PARAMS_PARAM(param));

    int dim = 0;
    while (ids != NULL) {
      dim++;
      ids = IDS_NEXT(ids);
    }

    Parameter p = {.type = PARAM_TYPE(PARAMS_PARAM(param)), .dim = dim};
    list[i] = p;

    i++;
    param = PARAMS_PARAMS(param);
  }

  Parameters params = {.arity = arity, .list = list};

  Function fun = {
      .name = STRcpy(func_name), .return_type = type, .params = params};
  return fun;
}

node_st *FNSprogram(node_st *node) {
  struct data_fns *data = DATA_FNS_GET();
  data->functions = create_funtable(NULL); // globale Ebene

  TRAVchildren(node);

  PROGRAM_FUNCTIONS(node) = data->functions;
  return node;
}

node_st *FNSfundef(node_st *node) {
  struct data_fns *data = DATA_FNS_GET();

  node_st *header = FUNDEF_HEADER(node);
  Function fun = from_header(header, false);
  funtable_insert(data->functions, fun);

  FunctionTable *functions = create_funtable(data->functions);
  FunctionTable *saved = data->functions;
  data->functions = functions;

  TRAVchildren(node);

  FUNDEF_FUNCTIONS(node) = functions;
  data->functions = saved;

  return node;
}

node_st *FNSfundec(node_st *node) {
  TRAVchildren(node);

  struct data_fns *data = DATA_FNS_GET();

  node_st *header = FUNDEC_HEADER(node);
  Function fun = from_header(header, false);
  funtable_insert(data->functions, fun);

  return node;
}
