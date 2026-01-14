#pragma once

#include <stdbool.h>

typedef struct Function {
  char *name;
} Function;

typedef struct FunctionTable {
  Function *functions;
  int size;     // Aktuelle Anzahl
  int capacity; // Maximale Kapazität
  struct FunctionTable *parent;
} FunctionTable;

FunctionTable *create_funtable(FunctionTable *parent);
void funtable_insert(FunctionTable *table, Function fun);
void funtable_free(FunctionTable *table);
bool funtable_contains(FunctionTable *table, char *name);
