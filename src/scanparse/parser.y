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

%code requires {
  typedef struct {
    node_st *vars;
    node_st *funs;
  } localdefs_st;
}

%union {
 char               *id;
 int                 cint;
 float               cflt;
 enum DeclarationType ctype;
 node_st             *node;
 localdefs_st         localdefs;
}

%locations

%token BRACKET_L BRACKET_R COMMA SEMICOLON CURLBRACKET_L CURLBRACKET_R SQUAREBRACKET_L SQUAREBRACKET_R
%token MINUS PLUS STAR SLASH PERCENT LE LT GE GT EQ NE OR AND NOT
%token TRUEVAL FALSEVAL LET RETURNSTATEMENT
%token IFSTATEMENT ELSESTATEMENT
%token WHILESTATEMENT DOSTATEMENT FORSTATEMENT
%token INTTYPE FLOATTYPE BOOLTYPE VOIDTYPE
%token EXTERN EXPORT

%token <cint> NUM
%token <cflt> FLOAT
%token <id> ID

%type <node> decls decl fundec fundef globaldec globaldef ids funheader funbody funparamdef
%type <node> intval floatval boolval constant expr exprs funcall varref localfundef localfundefs arrexpr
%type <node> stmts stmt vardef assign var program ifstatement block param params
%type <node> whilestatement dostatement forstatement returnstatement
%type <ctype> decltype voidtype
%type <localdefs> localdefs

%precedence "then"
%precedence ELSESTATEMENT
%left OR
%left AND
%left EQ NE
%left LT LE GT GE
%left PLUS MINUS
%left STAR SLASH PERCENT
%precedence "monop"

%start program

%%

program: decls { parseresult = ASTprogram($1); } ;

decls:
    decl decls { $$ = ASTdecls($1, $2); }
  | decl       { $$ = ASTdecls($1, NULL); }
  ;

decl:
     fundec { $$ = $1; }
  |  fundef { $$ = $1; }
  | globaldec { $$ = $1; }
  | globaldef { $$ = $1; }
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
      | returnstatement {
        $$ = $1;
      }
      | funcall SEMICOLON
      {
        $$ = ASTfuncallstmt($1);
        
      }
      ;

fundec: EXTERN funheader[header] SEMICOLON
                 {
                   $$ = ASTfundec($header);
                 };

fundef: funheader[header] CURLBRACKET_L funbody[body] CURLBRACKET_R
                 {
                    $$ = ASTfundef($header, $body, false);

                  }
       | EXPORT funheader[header] CURLBRACKET_L funbody[body] CURLBRACKET_R
                 {
                    $$ = ASTfundef($header, $body, true);

                  };

localdefs: vardef {
            $$ = (localdefs_st){.vars = ASTvardefs($1, NULL), .funs = NULL};
          }
          | localfundef {
            $$ = (localdefs_st){.vars = NULL, .funs = ASTfundefs($1, NULL)};
          }
          | vardef localdefs {
            $$ = (localdefs_st){.vars = ASTvardefs($1, $2.vars), .funs = $2.funs };
          } 
          | localfundef localfundefs {
            $$ = (localdefs_st){.vars = NULL, .funs = ASTfundefs($1, $2)};
          }
          ;

localfundef: funheader[header] CURLBRACKET_L funbody[body] CURLBRACKET_R
                 {
                    $$ = ASTfundef($header, $body, false);
                  };

localfundefs:
    localfundef localfundefs { $$ = ASTfundefs($1, $2); }
  | localfundef         {  $$ = ASTfundefs($1, NULL); }
  ;

funheader: decltype ID[name] BRACKET_L funparamdef[param] BRACKET_R {
          $$ = ASTfunheader($param, $1, $name);
        }
        | voidtype ID[name] BRACKET_L funparamdef[param] BRACKET_R {
          $$ = ASTfunheader($param, $1, $name);
        };

globaldec: EXTERN decltype ID SEMICOLON { $$ = ASTglobaldec(NULL, $2, $3); }
         | EXTERN decltype SQUAREBRACKET_L ids SQUAREBRACKET_R ID SEMICOLON { $$ = ASTglobaldec($4, $2, $6); };

globaldef: EXPORT vardef { $$ = ASTglobaldef($2, true); }
         | vardef        { $$ = ASTglobaldef($1, false); }
         ;

funparamdef:
    params { $$ = $1; }
  | %empty { $$ = NULL; }
  ;

ids:
    ID COMMA ids { $$ = ASTids($3, $1); }
  | ID          { $$ = ASTids(NULL, $1); }
  ;

params:
    param COMMA params { $$ = ASTparams($1, $3); }
  | param              { $$ = ASTparams($1, NULL); }
  ;

param: decltype ID 
        { 
          $$ = ASTparam(NULL, $1, $2); 
        }
        | decltype SQUAREBRACKET_L ids SQUAREBRACKET_R ID[name] 
        { 
          $$ = ASTparam($3, $1, $name); 
        };

funbody: localdefs stmts {
          $$ = ASTfunbody($1.vars, $1.funs, $2);
        }
        | localdefs {
          $$ = ASTfunbody($1.vars, $1.funs, NULL);
        }
        | stmts {
          $$ = ASTfunbody(NULL, NULL, $1);
        }
        | %empty { 
          $$ = ASTfunbody(NULL, NULL, NULL); 
        };

ifstatement: IFSTATEMENT BRACKET_L expr BRACKET_R block[block1] ELSESTATEMENT block[block2] {
          $$ = ASTifelsestatement($block1, $expr, $block2);
        }
        | IFSTATEMENT BRACKET_L expr BRACKET_R block %prec "then" {
          $$ = ASTifstatement($block, $expr);
        }
        ;

whilestatement: WHILESTATEMENT BRACKET_L expr BRACKET_R block {
          $$ = ASTwhilestatement($block, $expr);
        }
        ;

dostatement: DOSTATEMENT block WHILESTATEMENT BRACKET_L expr BRACKET_R SEMICOLON {
          $$ = ASTdostatement($block, $expr);
      };

forstatement: FORSTATEMENT BRACKET_L INTTYPE ID[variable] LET expr[init] COMMA expr[until] COMMA expr[step] BRACKET_R block {
          $$ = ASTforstatement($init, $until, $step, $block, $variable);
       }
       | FORSTATEMENT BRACKET_L INTTYPE ID[variable] LET expr[init] COMMA expr[until] BRACKET_R block {
          $$ = ASTforstatement($init, $until, NULL, $block, $variable);
       }
       ;

returnstatement: RETURNSTATEMENT expr SEMICOLON{
    $$ = ASTreturnstatement($expr);
   } 
  | RETURNSTATEMENT SEMICOLON{
    $$ = ASTreturnstatement(NULL);
  }
   ;

vardef: decltype[type] ID[name] LET expr SEMICOLON
       {
          $$ = ASTvardef($expr, NULL, $type, $name);
       }
       | decltype[type] ID[name] SEMICOLON
       {
          $$ = ASTvardef(NULL, NULL, $type, $name);
       }
       | decltype[type] SQUAREBRACKET_L exprs SQUAREBRACKET_R ID[name] LET expr SEMICOLON
       {
          $$ = ASTvardef($expr, $exprs, $type, $name);
       }
       | decltype[type] SQUAREBRACKET_L exprs SQUAREBRACKET_R ID[name] SEMICOLON
       {
          $$ = ASTvardef(NULL, $exprs, $type, $name);
       }
       ;

decltype: INTTYPE { $$ = TY_int; }
    | FLOATTYPE { $$ = TY_float; }
    | BOOLTYPE { $$ = TY_bool; }
    ;

voidtype: VOIDTYPE { $$ = TY_void; };

assign: var LET expr SEMICOLON
        {
          $$ = ASTassign($1, $3);
        }
        ;

var: ID { 
          $$ = ASTvar(NULL, $1); 
        }
        | ID SQUAREBRACKET_L exprs SQUAREBRACKET_R {
          $$ = ASTvar($exprs, $1);
        }  
        ;

varref: var { $$ = ASTvarref($1); }

expr:
    BRACKET_L decltype BRACKET_R expr %prec "monop" { $$ = ASTtypecast($4, $2); }
  | MINUS expr %prec "monop"                     { $$ = ASTmonop($2, MO_neg); }
  | NOT expr %prec "monop"                      { $$ = ASTmonop($2, MO_not); }
  | expr STAR expr                               { $$ = ASTbinop($1, $3, BO_mul); }
  | expr SLASH expr                              { $$ = ASTbinop($1, $3, BO_div); }
  | expr PERCENT expr                            { $$ = ASTbinop($1, $3, BO_mod); }
  | expr PLUS expr                               { $$ = ASTbinop($1, $3, BO_add); }
  | expr MINUS expr                              { $$ = ASTbinop($1, $3, BO_sub); }
  | expr LT expr                                 { $$ = ASTbinop($1, $3, BO_lt); }
  | expr LE expr                                 { $$ = ASTbinop($1, $3, BO_le); }
  | expr GT expr                                 { $$ = ASTbinop($1, $3, BO_gt); }
  | expr GE expr                                 { $$ = ASTbinop($1, $3, BO_ge); }
  | expr EQ expr                                 { $$ = ASTbinop($1, $3, BO_eq); }
  | expr NE expr                                 { $$ = ASTbinop($1, $3, BO_ne); }
  | expr AND expr                                { $$ = ASTbinop($1, $3, BO_and); }
  | expr OR expr                                 { $$ = ASTbinop($1, $3, BO_or); }
  | BRACKET_L expr BRACKET_R                         { $$ = $2; }
  | constant                                      { $$ = $1; }
  | varref                                       { $$ = $1; }
  | funcall                                      { $$ = $1; }
  ;

funcall:
    ID BRACKET_L BRACKET_R        { $$ = ASTfuncall(NULL, $1); }
  | ID BRACKET_L exprs BRACKET_R  { $$ = ASTfuncall($3, $1); }
  ;

exprs:
    expr COMMA exprs { $$ = ASTexprs($1, $3); }
  | expr { $$ = ASTexprs($1, NULL); }
  ;

block:
    CURLBRACKET_L stmts CURLBRACKET_R { $$ = ASTblock($stmts); }
  | CURLBRACKET_L CURLBRACKET_R              { $$ = ASTblock(NULL); }
  | stmt                               { $$ = ASTblock(ASTstmts($stmt, NULL)); }
  ;

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
