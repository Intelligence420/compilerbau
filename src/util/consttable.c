#include "util/consttable.h"
#include "ccngen/enum.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "palm/memory.h"
#include "palm/str.h"
#include <stdbool.h>

#define INITIAL_CAPACITY 2

ConstantTable *create_consttable(void) {
  ConstantTable *table = MEMmalloc(sizeof(ConstantTable));
  table->constants = MEMmalloc(INITIAL_CAPACITY * sizeof(Constant));
  table->size = 0;
  table->capacity = INITIAL_CAPACITY;
  return table;
}

static void grow_table(ConstantTable *table) {
  table->capacity = table->capacity * 2;
  table->constants =
      MEMrealloc(table->constants, table->capacity * sizeof(Constant));
}

int consttable_insert(ConstantTable *table, Constant con) {
  for (int i = 0; i < table->size; i++) {
    Constant entry = table->constants[i];
    if (con.type == entry.type &&
        ((con.type == TY_int && con.cint == entry.cint) ||
         (con.type == TY_float && con.cflt == entry.cflt))) {
      return i;
    }
  }

  if (table->size == table->capacity) {
    grow_table(table);
  }

  int idx = table->size;
  table->constants[idx] = con;
  table->size++;

  return idx;
}

void consttable_free(ConstantTable *table) {
  MEMfree(table->constants);
  MEMfree(table);
}
