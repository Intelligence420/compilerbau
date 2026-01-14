#pragma once

#include <stdbool.h>

typedef struct Variable {
    char *name;
} Variable;

typedef struct VariableTable {
    Variable *variables;
    int size; //Aktuelle Anzahl
    int capacity; //Maximale Kapazität
    struct VariableTable *parent;
} VariableTable;

VariableTable *create_vartable(VariableTable *parent);
void vartable_insert(VariableTable *table, Variable var);
void vartable_free(VariableTable *table);
bool vartable_contains(VariableTable *table, char *name);
