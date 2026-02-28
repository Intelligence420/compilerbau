#pragma once

#include "ccngen/enum.h"
#include <stdbool.h>

typedef struct Variable {
  char *name;
  enum DeclarationType type;
  int dim;
  // TOOD:
  bool isextern;
  bool valid;
  bool readonly;
} Variable;

enum ReferenzLevel {
  REF_LOCAL,
  REF_GLOBAL,
  REF_EXTERN, 
};

// TODO:
typedef struct VarReferenz {
  enum ReferenzLevel reftype;
  enum DeclarationType type;
  int dim;
  int n; // wie viele tabellen weiter oben
  int l; // platz in der tabelle (welche vielleicht weiter oben ist) lokalen tabelle
  bool readonly;
} VarReferenz;

typedef struct VariableTable {
  Variable *variables;
  int size;     // Aktuelle Anzahl
  int capacity; // Maximale Kapazität
  struct VariableTable *parent;
} VariableTable;

VariableTable *create_vartable(VariableTable *parent);
int vartable_insert(VariableTable *table, Variable var);
int vartable_insert_nocheck(VariableTable *table, Variable var);
void vartable_free(VariableTable *table);
bool vartable_contains(VariableTable *table, char *name);
VarReferenz *return_varref(VariableTable *table, char *name);
VarReferenz *return_varref_ignore_valid(VariableTable *table, char *name);
