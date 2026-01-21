#include "util/vartable.h"
#include "palm/ctinfo.h"
#include "palm/memory.h"
#include "palm/str.h"
#include <stdbool.h>

static Variable *vartable_local_get_variable(VariableTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (STReq(name, table->variables[i].name)) {
      return &table->variables[i];
    }
  }
  return NULL;
}

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
  // TODO: schauen ob es die variable schon gibt
  // NOTE: aber nur auf diesem level!!!
  /*
    int main(){
      int x = 0;
      int x = 1;
      }

      --> verboten!
    ------------------------------
    int x = 0;
    int main(){
      int x = 1;
      }

      --> Erlaubt!
  */
  Variable *existing_var = vartable_local_get_variable(table, var.name);
  if (existing_var != NULL) {
    CTI(CTI_ERROR, true, "variable with name %s already declared", var.name);
  }

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

Variable *vartable_get_variable(VariableTable *table, char *name) {
  Variable *var = vartable_local_get_variable(table, name);
  if (var != NULL) {
    Variable *new = MEMmalloc(sizeof(Variable));
    *new = *var;
    return new;
  }

  if (table->parent == NULL) {
    return NULL;
  } else {
    return vartable_get_variable(table->parent, name);
  }
}
