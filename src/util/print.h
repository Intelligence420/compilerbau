#pragma once

#include "ccngen/enum.h"
#include "palm/dbug.h"
#include <stdbool.h>

static char *TYstr(enum DeclarationType ty) {
  switch (ty) {
  case TY_int:
    return "int";
  case TY_float:
    return "float";
  case TY_bool:
    return "bool";
  case TY_void:
    return "void";
  default:
    DBUG_ASSERT(true, "unknown type detected");
    return NULL;
  }
}