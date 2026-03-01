#include "util/vartable.h"
#include "palm/ctinfo.h"
#include "palm/memory.h"
#include "palm/str.h"
#include <stdbool.h>

static Variable *vartable_local_get_variable(VariableTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (table->variables[i].valid && STReq(name, table->variables[i].name)) {
      return &table->variables[i];
    }
  }
  return NULL;
}

static int vartable_local_get_variable_idx(VariableTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (table->variables[i].valid && STReq(name, table->variables[i].name)) {
      return i;
    }
  }
  return -1;
}

static Variable *vartable_local_get_variable_all(VariableTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (STReq(name, table->variables[i].name)) {
      return &table->variables[i];
    }
  }
  return NULL;
}

static int vartable_local_get_variable_idx_all(VariableTable *table, char *name) {
  for (int i = 0; i < table->size; i++) {
    if (STReq(name, table->variables[i].name)) {
      return i;
    }
  }
  return -1;
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

int vartable_insert_nocheck(VariableTable *table, Variable var) {
  if (table->size == table->capacity) {
    grow_table(table);
  }

  var.valid = true;
  var.readonly = false;

  int idx = table->size;
  table->variables[idx] = var;
  table->size++;
  return idx;
}

int vartable_insert(VariableTable *table, Variable var) {
  Variable *existing_var = vartable_local_get_variable(table, var.name);
  if (existing_var != NULL) {
    CTI(CTI_ERROR, true, "variable with name %s already declared", var.name);
  }

  return vartable_insert_nocheck(table, var);
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

VarReferenz *return_varref(VariableTable *table, char *name) {
  VariableTable *curr = table;
  int j = 0;
  while (curr != NULL) {
    Variable *var = vartable_local_get_variable(curr, name);
    if (var != NULL) {
      VarReferenz *varref = MEMmalloc(sizeof(VarReferenz));
      varref->type = var->type;
      varref->dim = var->dim;
      varref->n = j;
      varref->l = vartable_local_get_variable_idx(curr, name);
      varref->readonly = var->readonly;
      if (var->isextern) {
        varref->reftype = REF_EXTERN;
      } else if (curr->parent == NULL) {
        varref->reftype = REF_GLOBAL;
      } else {
        varref->reftype = REF_LOCAL;
      }
      return varref;
    }
    curr = curr->parent;
    j++;
  }

  return NULL;
}

VarReferenz *return_varref_ignore_valid(VariableTable *table, char *name) {
  VariableTable *curr = table;
  int j = 0;
  while (curr != NULL) {
    Variable *var = vartable_local_get_variable_all(curr, name);
    if (var != NULL) {
      VarReferenz *varref = MEMmalloc(sizeof(VarReferenz));
      varref->type = var->type;
      varref->dim = var->dim;
      varref->n = j;
      varref->l = vartable_local_get_variable_idx_all(curr, name);
      varref->readonly = var->readonly;
      if (var->isextern) {
        varref->reftype = REF_EXTERN;
      } else if (curr->parent == NULL) {
        varref->reftype = REF_GLOBAL;
      } else {
        varref->reftype = REF_LOCAL;
      }
      return varref;
    }
    curr = curr->parent;
    j++;
  }

  return NULL;
}
Variable *vartable_get_variable_ptr(VariableTable *table, char *name) {
  Variable *var = vartable_local_get_variable_all(table, name);
  if (var != NULL) {
    return var;
  }

  if (table->parent == NULL) {
    return NULL;
  } else {
    return vartable_get_variable_ptr(table->parent, name);
  }
}
