%{

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "parser.h"
#include "palm/str.h"
#include "global/globals.h"
#include "palm/ctinfo.h"

// Use for error messages, see parser.y
extern int yyerror(char *errname);

#define FILTER(token) \
  global.col += yyleng;  \
  return( token);

#define LINEBUF_SIZE 256
#define YY_NO_UNPUT
#define YY_NO_INPUT

static inline void token_action();

int yycolumn = 1;
#define YY_USER_ACTION token_action();


%}

%p 6000 /* set positions to 6000 */
%e 2000 /* set nodes to 1100 */
%option noyywrap
%option nounput
%option yylineno

%x COMMENT

exponent [eE][+-]?[0-9]+

%%

"("                        { FILTER( BRACKET_L); }
")"                        { FILTER( BRACKET_R); }
"{"                        { FILTER( CURLBRACKET_L); }
"}"                        { FILTER( CURLBRACKET_R); }
","                        { FILTER( COMMA); }
";"                        { FILTER( SEMICOLON); }
"["                        { FILTER( SQUAREBRACKET_L); }
"]"                        { FILTER( SQUAREBRACKET_R); }

"-"                        { FILTER( MINUS); }
"+"                        { FILTER( PLUS);  }
"*"                        { FILTER( STAR);  }
"/"                        { FILTER( SLASH);  }
"%"                        { FILTER( PERCENT);  }

"<="                       { FILTER( LE);    }
"<"                        { FILTER( LT);    }
">="                       { FILTER( GE);    }
">"                        { FILTER( GT);    }
"=="                       { FILTER( EQ);    }
"!="                       { FILTER( NE);    }
"&&"                       { FILTER( AND);    }
"||"                       { FILTER( OR);    }

"!"                        { FILTER( NOT);    }

"="                        { FILTER( LET);    }

"true"                     { FILTER( TRUEVAL); }
"false"                    { FILTER( FALSEVAL); }

"if"                        { FILTER( IFSTATEMENT); }
"else"                      { FILTER( ELSESTATEMENT); }

"while"                     { FILTER( WHILESTATEMENT); }
"do"                        { FILTER( DOSTATEMENT); }
"for"                       { FILTER( FORSTATEMENT); }

"int"                       { FILTER( INTTYPE); }
"float"                     { FILTER( FLOATTYPE); }
"bool"                      { FILTER( BOOLTYPE); }
"void"                      { FILTER( VOIDTYPE); }

"extern"                    { FILTER( EXTERN); }
"export"                    { FILTER( EXPORT); }

"return"                    { FILTER( RETURNSTATEMENT);}

[A-Za-z][A-Za-z0-9_]*      { yylval.id = STRcpy(yytext);
                             FILTER( ID);
                           }

\/\/.*                     {}

"/*"                       { BEGIN(COMMENT); }
<COMMENT>([^*]|\*[^/])*    { global.col += yyleng; }
<COMMENT>\n.*              { yycolumn = 1;
                             global.line += 1;
                             global.col = 0;
                             yyless(1); }
<COMMENT>"*/"              { global.col += yyleng;
                             BEGIN(INITIAL); }
"0x"[0-9a-fA-F]+           |
[0-9]+                     { errno = 0;
                             char *end;
                             long cint = strtol(yytext, &end, 0);

                             if (errno == ERANGE) {
                                 yyerror("integer literal out of range");
                             } else if (*end != '\0') {
                                 yyerror("invalid integer literal");
                             } else if (cint < INT_MIN || cint > INT_MAX) {
                                 yyerror("integer literal out of range");
                             }

                             yylval.cint = cint;
                             FILTER(NUM); }

[0-9]+{exponent}           |
[0-9]+\.[0-9]*{exponent}?  |
\.[0-9]+{exponent}?        {
                             errno = 0;
                             char *end;
                             yylval.cflt = strtof(yytext, &end);

                             if (errno == ERANGE) {
                                 yyerror("float literal out of range");
                             } else if (*end != '\0') {
                                 yyerror("invalid float literal");
                             }

                             FILTER(FLOAT); }
\n.*                       {
                             yycolumn = 1;
                             global.line += 1;
                             global.col = 0;
                             yyless(1);
                           }

[ \t]                      { global.col += yyleng;
                           }
%%

static inline void token_action() {
    yylloc.first_line = yylloc.last_line = yylineno;
    yylloc.first_column = yycolumn;
    yylloc.last_column = yycolumn + yyleng - 1;
    yycolumn += yyleng;
}
