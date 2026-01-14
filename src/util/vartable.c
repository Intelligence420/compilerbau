#include "util/vartable.h"
#include "palm/memory.h"
#include "palm/str.h"
#include <stdbool.h>

#define INITIAL_CAPACITY 2

VariableTable *create_vartable(VariableTable *parent) {
  VariableTable *table = MEMmalloc(sizeof(VariableTable));
  table->variables = MEMmalloc(INITIAL_CAPACITY * sizeof(Variable));
  table->size = 0;
  table->capacity = INITIAL_CAPACITY;
  table->parent = parent;
  return table;
}

static void grow_table(VariableTable *table) {
  table->capacity = table->capacity * 2;
  table->variables =
      MEMrealloc(table->variables, table->capacity * sizeof(Variable));
}

void vartable_insert(VariableTable *table, Variable var) {
  if (table->size == table->capacity) {
    grow_table(table);
  }

  table->variables[table->size] = var;
  table->size++;
}

void vartable_free(VariableTable *table) {
  for (int i = 0; i < table->size; i++) {
    MEMfree(table->variables[i].name);
  }

  MEMfree(table->variables);
  MEMfree(table);
}

bool vartable_contains(VariableTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (STReq(name, table->variables[i].name)) {
      return true;
    }
  }

  if (table->parent == NULL) {
    return false;
  } else {
    return vartable_contains(table->parent, name);
  }
}