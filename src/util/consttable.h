#pragma once

#include "ccngen/enum.h"

typedef struct Constant {
  enum DeclarationType type;
  int cint;
  float cflt;
} Constant;

typedef struct ConstantTable {
  Constant *constants;
  int size;     // Aktuelle Anzahl
  int capacity; // Maximale Kapazität
} ConstantTable;

ConstantTable *create_consttable(void);
int consttable_insert(ConstantTable *table, Constant con);
void consttable_free(ConstantTable *table);
