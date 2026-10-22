%{
/* Analizador lexico de Cringe (Flex).
   Las reglas siguen la seccion 4 del documento del lenguaje. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "ast.h"
#include "parser.h"
#include "errors.h"

/* Posicion actual (linea y columna del siguiente caracter a leer) */
static int cur_line = 1, cur_col = 1;
/* Fin del ultimo token devuelto: se usa para ubicar el error "fin del archivo" */
static int prev_line = 1, prev_col = 0;

/* Se ejecuta antes de cada regla: guarda la ubicacion del lexema para el parser */
#define YY_USER_ACTION \
    { \
        yylloc.first_line = cur_line; \
        yylloc.first_column = cur_col; \
        yylloc.last_line = cur_line; \
        yylloc.last_column = cur_col + yyleng - 1; \
        cur_col += yyleng; \
    }

#define RETURN_TOKEN(t) \
    do { \
        prev_line = yylloc.last_line; \
        prev_col = yylloc.last_column; \
        return (t); \
    } while (0)

/* Los errores lexicos detienen el analisis en el primero */
#define LEX_ERROR(...) \
    do { \
        report_error("error léxico", yylloc.first_line, yylloc.first_column, __VA_ARGS__); \
        exit(COD_LEXICO); \
    } while (0)
%}

%option noyywrap nounput noinput

DIGITO      [0-9]
LETRA       [a-zA-Z]

%%

"invoco"        { RETURN_TOKEN(INVOCO); }
"trans"    { RETURN_TOKEN(TRANS); }
"diay"      { RETURN_TOKEN(DIAY); }
"uwu"           { RETURN_TOKEN(UWU); }
"shi"           { RETURN_TOKEN(SHI); }
"sino"          { RETURN_TOKEN(SINO); }
"hechizo"       { RETURN_TOKEN(CHAMBA); }
"lanza"         { RETURN_TOKEN(DA); }
"vacio"         { RETURN_TOKEN(NADA); }
"regreso"       { RETURN_TOKEN(DEVUELVO); }
"narro"         { RETURN_TOKEN(MUESTRO); }
"numerito"         { RETURN_TOKEN(T_NUMERITO); }
"flotadito"          { RETURN_TOKEN(T_FLOTADITO); }
"pergamino"     { RETURN_TOKEN(T_FRASE); }
"vibra"         { RETURN_TOKEN(T_VIBRA); }

"chi"|"nio"     { yylval.str = strdup(yytext); RETURN_TOKEN(LIT_VIBRA); }

{LETRA}({LETRA}|{DIGITO}|_)*   { yylval.str = strdup(yytext); RETURN_TOKEN(IDENTIFICADOR); }

{DIGITO}+"."{DIGITO}+ {
    errno = 0;
    double d = strtod(yytext, NULL);
    if (errno == ERANGE && isinf(d))
        LEX_ERROR("flotadito fuera de rango: %s (el cristal de maná explotó)", yytext);
    yylval.str = strdup(yytext);
    RETURN_TOKEN(LIT_FLOTADITO);
}

{DIGITO}+ {
    errno = 0;
    strtoll(yytext, NULL, 10);
    if (errno == ERANGE)
        LEX_ERROR("numerito fuera de rango: %s (ni el héroe más roto llega tan alto)", yytext);
    yylval.str = strdup(yytext);
    RETURN_TOKEN(LIT_NUMERITO);
}

\"[^"\n]*\" {
    /* se guarda el texto sin las comillas */
    yylval.str = strndup(yytext + 1, yyleng - 2);
    RETURN_TOKEN(LIT_FRASE);
}

\"[^"\n]* {
    LEX_ERROR("pergamino sin cerrar: falta la comilla de cierre (el hechizo se escapó)");
}

"=="            { RETURN_TOKEN(EQ); }
"!="            { RETURN_TOKEN(NE); }
"<="            { RETURN_TOKEN(LE); }
">="            { RETURN_TOKEN(GE); }
"<"             { RETURN_TOKEN(LT); }
">"             { RETURN_TOKEN(GT); }
"&&"            { RETURN_TOKEN(AND); }
"||"            { RETURN_TOKEN(OR); }
"!"             { RETURN_TOKEN(NOT); }

"("             { RETURN_TOKEN('('); }
")"             { RETURN_TOKEN(')'); }
"{"             { RETURN_TOKEN('{'); }
"}"             { RETURN_TOKEN('}'); }
","             { RETURN_TOKEN(','); }

"//"[^\n]*      { /* comentario de linea: se ignora */ }

[ \t\r]+        { /* espacios: se ignoran */ }

\n              { cur_line++; cur_col = 1; }

[\xC2-\xF4][\x80-\xBF]* {
    LEX_ERROR("carácter no reconocido '%s' (solo se aceptan runas ASCII en este reino)", yytext);
}

. {
    LEX_ERROR("símbolo no reconocido '%s' (esa runa no existe en este reino)", yytext);
}

<<EOF>> {
    /* el fin del archivo se ubica justo despues del ultimo token */
    yylloc.first_line = yylloc.last_line = prev_line;
    yylloc.first_column = yylloc.last_column = prev_col + 1;
    return 0;
}

%%
