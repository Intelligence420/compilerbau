#pragma once

#include "ccngen/enum.h"
#include <stdbool.h>

typedef struct Variable {
    char *name;
    enum DeclarationType type;
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
Variable *vartable_get_variable(VariableTable *table, char *name);
