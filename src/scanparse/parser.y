%{


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "palm/memory.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "palm/str.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"

static node_st *parseresult = NULL;
extern int yylex();
int yyerror(char *errname);
extern FILE *yyin;
void AddLocToNode(node_st *node, void *begin_loc, void *end_loc);


%}

%union {
 char               *id;
 int                 cint;
 float               cflt;
 bool                cbool;
 enum BinOpType     cbinop;
 enum DeclarationType ctype;
 node_st             *node;
}

%locations

%token BRACKET_L BRACKET_R COMMA SEMICOLON CURLBRACKET_L CURLBRACKET_R SQUAREBRACKET_L SQUAREBRACKET_R
%token MINUS PLUS STAR SLASH PERCENT LE LT GE GT EQ NE OR AND NOT
%token TRUEVAL FALSEVAL LET 
%token IFSTATEMENT ELSESTATEMENT
%token WHILESTATEMENT DOSTATEMENT FORSTATEMENT
%token INTTYPE FLOATTYPE BOOLTYPE VOIDTYPE

%token <cint> NUM
%token <cflt> FLOAT
%token <cbool> BOOL
%token <id> ID

%type <node> intval floatval boolval constant expr
%type <node> stmts stmt declaration assign varlet program voiddeclaration ifstatement block
%type <node> whilestatement dostatement forstatement
%type <cbinop> binop
%type <ctype> decltype voidtype

%start program

%%

program: stmts
         {
           parseresult = ASTprogram($1);
         }
         ;

stmts: stmt stmts
        {
          $$ = ASTstmts($1, $2);
        }
      | stmt
        {
          $$ = ASTstmts($1, NULL);
        }
        ;

stmt: assign
       {
         $$ = $1;
       }
       |
       declaration
       {
         $$ = $1;
       }
       | voiddeclaration
       {
         $$ = $1;
       }
       | ifstatement
       {
        $$ = $1;
       }
       | whilestatement
       {
         $$ = $1;
       }
       | dostatement
       {
          $$ = $1;
       }
       | forstatement
       {
         $$ = $1;
       }
       ;

ifstatement: IFSTATEMENT BRACKET_L expr[expr] BRACKET_R block[block] ELSESTATEMENT block[block2] {
          $$ = ASTifelsestatement($block, $expr, $block2);
        }
        | IFSTATEMENT BRACKET_L expr[expr] BRACKET_R block[block]{
          $$ = ASTifstatement($block, $expr);
        }
        ;

whilestatement: WHILESTATEMENT BRACKET_L expr[expr] BRACKET_R block[block] {
          $$ = ASTwhilestatement($block, $expr);      
        }
        ;

dostatement: DOSTATEMENT block[block] WHILESTATEMENT BRACKET_L expr[expr] BRACKET_R SEMICOLON {
          $$ = ASTdostatement($block, $expr);
      };

forstatement: FORSTATEMENT BRACKET_L INTTYPE ID[variable] LET expr[init] COMMA expr[until] COMMA expr[step] BRACKET_R block[block] {
          $$ = ASTforstatement($init, $until, $step, $block, $variable);
       }
       | FORSTATEMENT BRACKET_L INTTYPE ID[variable] LET expr[init] COMMA expr[until] BRACKET_R block[block] {
          $$ = ASTforstatement($init, $until, NULL, $block, $variable);
       }
       ;

declaration: decltype[type] ID[name] LET constant[expr] SEMICOLON
       {
          $$ = ASTdeclaration($expr, $type, $name);
       }
       | decltype[type] ID[name] LET expr[expr] SEMICOLON
       {
          $$ = ASTdeclaration($expr, $type, $name);
       };


decltype: INTTYPE { $$ = TY_int; }
    | FLOATTYPE { $$ = TY_float; }
    | BOOLTYPE { $$ = TY_bool; }
    ;

voiddeclaration: voidtype[type] SEMICOLON
       {
        $$ = ASTvoiddeclaration($type);
       };

voidtype: VOIDTYPE { $$ = TY_void; };

assign: varlet LET expr SEMICOLON
        {
          $$ = ASTassign($1, $3);
        }
        ;

varlet: ID
        {
          $$ = ASTvarlet($1);
          AddLocToNode($$, &@1, &@1);
        }
        ;

expr: constant
      {
        $$ = $1;
      }
    | ID
      {
        $$ = ASTvar($1);
      }
    | expr[left] binop[type] expr[right]
      {
        $$ = ASTbinop( $left, $right, $type);
        AddLocToNode($$, &@left, &@right);
      }
    | BRACKET_L expr[experssion] BRACKET_R
    {
      $$ = $experssion;
    }
    ;

block: CURLBRACKET_L stmts[stmts] CURLBRACKET_R {
      $$ = ASTblock($stmts);
      };

constant: floatval
          {
            $$ = $1;
          }
        | intval
          {
            $$ = $1;
          }
        | boolval
          {
            $$ = $1;
          }
        ;

floatval: FLOAT
           {
             $$ = ASTfloat($1);
           }
         ;

intval: NUM
        {
          $$ = ASTnum($1);
        }
      ;

boolval: TRUEVAL
         {
           $$ = ASTbool(true);
         }
       | FALSEVAL
         {
           $$ = ASTbool(false);
         }
       ;

binop: ArithOp
      {
        $$ = $1;
      }
    | RelOp
      { 
        $$ = $1; 
      }
    | LogicalOp
      { 
        $$ = $1; 
      }
     ;

ArithOp:  PLUS    { $$ = BO_add; }
        | MINUS   { $$ = BO_sub; }
        | STAR    { $$ = BO_mul; }
        | SLASH   { $$ = BO_div; }
        | PERCENT { $$ = BO_mod; }
        ;

RelOp:  LE      { $$ = BO_le; }
      | LT      { $$ = BO_lt; }
      | GE      { $$ = BO_ge; }
      | GT      { $$ = BO_gt; }
      | EQ      { $$ = BO_eq; }
      | NE      { $$ = BO_ne; }
      ;

LogicalOp: OR    { $$ = BO_or; }
         | AND   { $$ = BO_and; }
         ;

MonOp: NOT  { $$ = BO_not; }
    |  MINUS { $$ = BO_uminus; }
     ;

%%

void AddLocToNode(node_st *node, void *begin_loc, void *end_loc)
{
    // Needed because YYLTYPE unpacks later than top-level decl.
    YYLTYPE *loc_b = (YYLTYPE*)begin_loc;
    YYLTYPE *loc_e = (YYLTYPE*)end_loc;
    NODE_BLINE(node) = loc_b->first_line;
    NODE_BCOL(node) = loc_b->first_column;
    NODE_ELINE(node) = loc_e->last_line;
    NODE_ECOL(node) = loc_e->last_column;
}

int yyerror(char *error)
{
  CTI(CTI_ERROR, true, "line %d, col %d\nError parsing source code: %s\n",
            global.line, global.col, error);
  CTIabortOnError();
  return 0;
}

node_st *SPdoScanParse(node_st *root)
{
    DBUG_ASSERT(root == NULL, "Started parsing with existing syntax tree.");
    yyin = fopen(global.input_file, "r");
    if (yyin == NULL) {
        CTI(CTI_ERROR, true, "Cannot open file '%s'.", global.input_file);
        CTIabortOnError();
    }
    yyparse();
    return parseresult;
}
