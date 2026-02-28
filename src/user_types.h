#pragma once

#define CCN_USES_UNSAFE_ACKNOWLEDGE

#include "palm/hash_table.h"
#include "util/funtable.h"
#include "util/vartable.h"
#include "util/consttable.h"
#include <stdio.h>

typedef htable_st* htable_stptr;
typedef FunctionTable* FunctionTablePtr;
typedef Function* FunctionPtr;
typedef VariableTable* VariableTablePtr;
typedef VarReferenz* VariablePtr;
typedef ConstantTable* ConstantTablePtr;
typedef FILE *file;

// Add more types here if necessary
