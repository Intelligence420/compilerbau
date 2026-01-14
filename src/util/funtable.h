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

FunctionTable *create_table(FunctionTable *parent);
void table_insert(FunctionTable *table, Function fun);
void table_free(FunctionTable *table);
bool table_contains(FunctionTable *table, char *name);
