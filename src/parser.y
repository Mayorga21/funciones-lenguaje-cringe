%{
/* Analizador sintactico de Cringe (Bison, LALR(1)).
   La gramatica sigue la seccion 5 del documento del lenguaje, ampliada con funciones.
   Cada regla construye su nodo del AST (traduccion dirigida por sintaxis). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "errors.h"

int yylex(void);
void yyerror(const char *msg);

/* Raiz del AST, la lee main despues de yyparse */
Node *g_root = NULL;

/* Nodo Llamada: hijos Nombre y Argumentos (se usa como sentencia y como expresion) */
static Node *make_call(char *name, int line, int col, Node *args)
{
    Node *call = ast_new(N_LLAMADA, NULL, line, col);
    ast_add_child(call, ast_new(N_NOMBRE, name, line, col));
    ast_add_child(call, args);
    return call;
}

%}

%code requires {
    #include "ast.h"
}

%locations
%define parse.error verbose

%union {
    char *str;
    Node *node;
}

%token END 0 "fin del archivo"
%token INVOCO "invoco"
%token TRANS "trans"
%token DIAY "diay"
%token UWU "uwu"
%token CHAMBA "hechizo"
%token DA "lanza"
%token NADA "vacio"
%token DEVUELVO "regreso"
%token MUESTRO "narro"
%token T_NUMERITO "numerito"
%token T_FLOTADITO "flotadito"
%token T_FRASE "pergamino"
%token T_VIBRA "vibra"
%token <str> LIT_VIBRA "valor vibra (chi o nio)"
%token <str> IDENTIFICADOR "nombre"
%token <str> LIT_NUMERITO "numero entero"
%token <str> LIT_FLOTADITO "numero decimal"
%token <str> LIT_FRASE "texto entre comillas"

/* Si el analisis sintactico falla, Bison descarta lo que ya habia construido:
   estos destructores liberan esa memoria. */
%destructor { free($$); } <str>
%destructor { ast_free($$); } <node>

%type <node> lista_sentencias sentencia declaracion asignacion tipo expresion literal
%type <node> funcion tipo_retorno parametros_opt lista_parametros parametro
%type <node> cuerpo sentencia_func retorno llamada_sent muestro_sent
%type <node> argumentos_opt lista_argumentos

%%

programa
    : lista_sentencias          { g_root = $1; }
    ;

lista_sentencias
    : lista_sentencias sentencia {
          ast_add_child($1, $2);
          $$ = $1;
      }
    | sentencia {
          $$ = ast_new(N_PROGRAMA, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $1);
      }
    ;

/* Sentencias del programa (numerito global). 'regreso' solo existe dentro de funciones. */
sentencia
    : declaracion               { $$ = $1; }
    | asignacion                { $$ = $1; }
    | funcion                   { $$ = $1; }
    | llamada_sent              { $$ = $1; }
    | muestro_sent              { $$ = $1; }
    ;

declaracion
    : INVOCO tipo IDENTIFICADOR UWU {
          $$ = ast_new(N_DECLARACION, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $2);
          ast_add_child($$, ast_new(N_NOMBRE, $3, @3.first_line, @3.first_column));
      }
    | INVOCO tipo IDENTIFICADOR DIAY expresion UWU {
          $$ = ast_new(N_DECLARACION, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $2);
          ast_add_child($$, ast_new(N_NOMBRE, $3, @3.first_line, @3.first_column));
          ast_add_child($$, $5);
      }
    ;

asignacion
    : IDENTIFICADOR TRANS expresion UWU {
          $$ = ast_new(N_ASIGNACION, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, ast_new(N_NOMBRE, $1, @1.first_line, @1.first_column));
          ast_add_child($$, $3);
      }
    ;

/* hechizo nombre(numerito a, pergamino b) lanza numerito { ... } */
funcion
    : CHAMBA IDENTIFICADOR '(' parametros_opt ')' DA tipo_retorno '{' cuerpo '}' {
          $$ = ast_new(N_FUNCION, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $7);
          ast_add_child($$, ast_new(N_NOMBRE, $2, @2.first_line, @2.first_column));
          ast_add_child($$, $4);
          ast_add_child($$, $9);
      }
    ;

tipo_retorno
    : tipo      { $$ = $1; }
    | NADA      { $$ = ast_new(N_TIPO, strdup("vacio"), @1.first_line, @1.first_column); }
    ;

parametros_opt
    : /* vacio */ { $$ = ast_new(N_PARAMETROS, NULL, @$.first_line, @$.first_column); }
    | lista_parametros { $$ = $1; }
    ;

lista_parametros
    : lista_parametros ',' parametro {
          ast_add_child($1, $3);
          $$ = $1;
      }
    | parametro {
          $$ = ast_new(N_PARAMETROS, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $1);
      }
    ;

parametro
    : tipo IDENTIFICADOR {
          $$ = ast_new(N_PARAMETRO, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $1);
          ast_add_child($$, ast_new(N_NOMBRE, $2, @2.first_line, @2.first_column));
      }
    ;

cuerpo
    : /* vacio */ { $$ = ast_new(N_CUERPO, NULL, @$.first_line, @$.first_column); }
    | cuerpo sentencia_func {
          ast_add_child($1, $2);
          $$ = $1;
      }
    ;

/* Sentencias dentro de una funcion: no se anidan funciones */
sentencia_func
    : declaracion               { $$ = $1; }
    | asignacion                { $$ = $1; }
    | llamada_sent              { $$ = $1; }
    | muestro_sent              { $$ = $1; }
    | retorno                   { $$ = $1; }
    ;

retorno
    : DEVUELVO expresion UWU {
          $$ = ast_new(N_RETORNO, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $2);
      }
    | DEVUELVO UWU {
          $$ = ast_new(N_RETORNO, NULL, @1.first_line, @1.first_column);
      }
    ;

llamada_sent
    : IDENTIFICADOR '(' argumentos_opt ')' UWU {
          $$ = make_call($1, @1.first_line, @1.first_column, $3);
      }
    ;

muestro_sent
    : MUESTRO expresion UWU {
          $$ = ast_new(N_MUESTRO, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $2);
      }
    ;

argumentos_opt
    : /* vacio */ { $$ = ast_new(N_ARGUMENTOS, NULL, @$.first_line, @$.first_column); }
    | lista_argumentos { $$ = $1; }
    ;

lista_argumentos
    : lista_argumentos ',' expresion {
          ast_add_child($1, $3);
          $$ = $1;
      }
    | expresion {
          $$ = ast_new(N_ARGUMENTOS, NULL, @1.first_line, @1.first_column);
          ast_add_child($$, $1);
      }
    ;

tipo
    : T_NUMERITO    { $$ = ast_new(N_TIPO, strdup("numerito"), @1.first_line, @1.first_column); }
    | T_FLOTADITO   { $$ = ast_new(N_TIPO, strdup("flotadito"), @1.first_line, @1.first_column); }
    | T_FRASE       { $$ = ast_new(N_TIPO, strdup("pergamino"), @1.first_line, @1.first_column); }
    | T_VIBRA       { $$ = ast_new(N_TIPO, strdup("vibra"), @1.first_line, @1.first_column); }
    ;

expresion
    : literal       { $$ = $1; }
    | IDENTIFICADOR { $$ = ast_new(N_NOMBRE, $1, @1.first_line, @1.first_column); }
    | IDENTIFICADOR '(' argumentos_opt ')' {
          $$ = make_call($1, @1.first_line, @1.first_column, $3);
      }
    ;

literal
    : LIT_NUMERITO  { $$ = ast_new(N_NUMERO, $1, @1.first_line, @1.first_column); }
    | LIT_FLOTADITO { $$ = ast_new(N_DECIMAL, $1, @1.first_line, @1.first_column); }
    | LIT_FRASE     { $$ = ast_new(N_FRASE, $1, @1.first_line, @1.first_column); }
    | LIT_VIBRA     { $$ = ast_new(N_VIBRA, $1, @1.first_line, @1.first_column); }
    ;

%%

/* Reemplaza todas las apariciones de 'from' por 'to' en 'src' y escribe en 'dst' */
static void replace_all(const char *src, const char *from, const char *to,
                        char *dst, size_t size)
{
    size_t lf = strlen(from), lt = strlen(to), n = 0;
    while (*src && n + lt + 1 < size) {
        if (strncmp(src, from, lf) == 0) {
            memcpy(dst + n, to, lt);
            n += lt;
            src += lf;
        } else {
            dst[n++] = *src++;
        }
    }
    dst[n] = '\0';
}

/* Bison genera mensajes en ingles ("syntax error, unexpected X, expecting Y"); se traducen */
void yyerror(const char *msg)
{
    char a[512], b[512], c[512];
    replace_all(msg, "syntax error, unexpected ", "se encontró ", a, sizeof a);
    replace_all(a, ", expecting ", ", se esperaba ", b, sizeof b);
    replace_all(b, " or ", " o ", c, sizeof c);
    report_error("error sintáctico", yylloc.first_line, yylloc.first_column,
                 "%s (conjuro mal recitado)", c);
}
