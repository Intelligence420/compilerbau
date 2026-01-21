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
#include <string.h>

#include "ccngen/trav_data.h"
#include "palm/str.h"
#include "util/funtable.h"

void FNSinit(void) {}
void FNSfini(void) {}

node_st *FNSprogram(node_st *node) {
  struct data_fns *data = DATA_FNS_GET();
  data->functions = create_funtable(NULL); // globale Ebene

  TRAVchildren(node);

  PROGRAM_FUNCTIONS(node) = data->functions;
  return node;
}

node_st *FNSfundef(node_st *node) {
  node_st *header = FUNDEF_HEADER(node);
  const char *func_name = FUNHEADER_NAME(header);
  enum DeclarationType type = FUNHEADER_TYPE(header);

  struct data_fns *data = DATA_FNS_GET();

  Function fun = {.name = STRcpy(func_name), .return_type = type};
  funtable_insert(data->functions, fun);

  FunctionTable *functions = create_funtable(data->functions);
  FunctionTable *saved = data->functions;
  data->functions = functions;

  TRAVchildren(node);

  FUNDEF_FUNCTIONS(node) = functions;
  data->functions = saved;

  return node;
}

node_st *FNSfundec(node_st *node){
  TRAVchildren(node);

  node_st *header = FUNDEC_HEADER(node);
  const char *func_name = FUNHEADER_NAME(header);
  enum DeclarationType type = FUNHEADER_TYPE(header);
  struct data_fns *data = DATA_FNS_GET();

  Function fun = {.name = STRcpy(func_name), .return_type = type};
  funtable_insert(data->functions, fun);

  return node;
}
