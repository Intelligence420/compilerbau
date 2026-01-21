#include "util/funtable.h"
#include "palm/memory.h"
#include "palm/str.h"
#include <stdbool.h>

#define INITIAL_CAPACITY 2

FunctionTable *create_funtable(FunctionTable *parent) {
  FunctionTable *table = MEMmalloc(sizeof(FunctionTable));
  table->functions = MEMmalloc(INITIAL_CAPACITY * sizeof(Function));
  table->size = 0;
  table->capacity = INITIAL_CAPACITY;
  table->parent = parent;
  return table;
}

static void grow_table(FunctionTable *table) {
  table->capacity = table->capacity * 2;
  table->functions =
      MEMrealloc(table->functions, table->capacity * sizeof(Function));
}

void funtable_insert(FunctionTable *table, Function fun) {
  if (table->size == table->capacity) {
    grow_table(table);
  }

  table->functions[table->size] = fun;
  table->size++;
}

void funtable_free(FunctionTable *table) {
  for (int i = 0; i < table->size; i++) {
    MEMfree(table->functions[i].name);
  }

  MEMfree(table->functions);
  MEMfree(table);
}

bool funtable_contains(FunctionTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (STReq(name, table->functions[i].name)) {
      return true;
    }
  }

  if (table->parent == NULL) {
    return false;
  } else {
    return funtable_contains(table->parent, name);
  }
}

static Function *funtable_local_get_function(FunctionTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (STReq(name, table->functions[i].name)) {
      return &table->functions[i];
    }
  }
  return NULL;
}

Function *funtable_get_function(FunctionTable *table, char *name) {
  Function *fun = funtable_local_get_function(table, name);
  if (fun != NULL) {
    Function *new = MEMmalloc(sizeof(Function));
    *new = *fun;
    return new;
  }

  if (table->parent == NULL) {
    return NULL;
  } else {
    return funtable_get_function(table->parent, name);
  }
}
