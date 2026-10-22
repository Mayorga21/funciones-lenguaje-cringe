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

/* Nodo binario con operador: Comparacion o Logica. La posicion es la del operador. */
static Node *make_binop(NodeKind kind, const char *op, int line, int col, Node *l, Node *r)
{
    Node *n = ast_new(kind, strdup(op), line, col);
    ast_add_child(n, l);
    ast_add_child(n, r);
    return n;
}

/* shi (cond) { ... } con un sino opcional; 'otro' puede ser NULL */
static Node *make_si(int line, int col, Node *cond, Node *entonces, Node *otro)
{
    Node *si = ast_new(N_SI, NULL, line, col);
    ast_add_child(si, cond);
    ast_add_child(si, entonces);
    if (otro)
        ast_add_child(si, otro);
    return si;
}

%}

%code requires {
    #include "ast.h"
}

%locations
%define parse.error custom

%union {
    char *str;
    Node *node;
}

%token END 0 "fin del archivo"
%token INVOCO "invoco"
%token TRANS "trans"
%token DIAY "diay"
%token UWU "uwu"
%token SHI "shi"
%token SINO "sino"
%token OR "||"
%token AND "&&"
%token NOT "!"
%token EQ "=="
%token NE "!="
%token LT "<"
%token GT ">"
%token LE "<="
%token GE ">="
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

/* Precedencia de menor a mayor: || , && , comparaciones, ! (como en C).
   Las comparaciones no se encadenan: a < b < c es un error de sintaxis. */
%left OR
%left AND
%nonassoc EQ NE LT GT LE GE
%right NOT

/* Si el analisis sintactico falla, Bison descarta lo que ya habia construido:
   estos destructores liberan esa memoria. */
%destructor { free($$); } <str>
%destructor { ast_free($$); } <node>

%type <node> lista_sentencias sentencia declaracion asignacion tipo expresion literal
%type <node> funcion tipo_retorno parametros_opt lista_parametros parametro
%type <node> cuerpo sentencia_func retorno llamada_sent muestro_sent
%type <node> argumentos_opt lista_argumentos
%type <node> si_g si_f bloque_g bloque_f sentencia_blq_g

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
    | si_g                      { $$ = $1; }
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
    | si_f                      { $$ = $1; }
    ;

/* shi (condicion) { ... } sino { ... }   (el sino es opcional y puede encadenarse: sino shi (...) { ... })
   Hay dos versiones: la global no admite 'regreso', la de dentro de un hechizo si. */
si_g
    : SHI '(' expresion ')' '{' bloque_g '}' {
          $$ = make_si(@1.first_line, @1.first_column, $3, $6, NULL);
      }
    | SHI '(' expresion ')' '{' bloque_g '}' SINO '{' bloque_g '}' {
          $10->kind = N_SINO;
          $$ = make_si(@1.first_line, @1.first_column, $3, $6, $10);
      }
    | SHI '(' expresion ')' '{' bloque_g '}' SINO si_g {
          Node *otro = ast_new(N_SINO, NULL, @8.first_line, @8.first_column);
          ast_add_child(otro, $9);
          $$ = make_si(@1.first_line, @1.first_column, $3, $6, otro);
      }
    ;

bloque_g
    : /* vacio */ { $$ = ast_new(N_BLOQUE, NULL, @$.first_line, @$.first_column); }
    | bloque_g sentencia_blq_g {
          ast_add_child($1, $2);
          $$ = $1;
      }
    ;

sentencia_blq_g
    : declaracion               { $$ = $1; }
    | asignacion                { $$ = $1; }
    | llamada_sent              { $$ = $1; }
    | muestro_sent              { $$ = $1; }
    | si_g                      { $$ = $1; }
    ;

si_f
    : SHI '(' expresion ')' '{' bloque_f '}' {
          $$ = make_si(@1.first_line, @1.first_column, $3, $6, NULL);
      }
    | SHI '(' expresion ')' '{' bloque_f '}' SINO '{' bloque_f '}' {
          $10->kind = N_SINO;
          $$ = make_si(@1.first_line, @1.first_column, $3, $6, $10);
      }
    | SHI '(' expresion ')' '{' bloque_f '}' SINO si_f {
          Node *otro = ast_new(N_SINO, NULL, @8.first_line, @8.first_column);
          ast_add_child(otro, $9);
          $$ = make_si(@1.first_line, @1.first_column, $3, $6, otro);
      }
    ;

bloque_f
    : /* vacio */ { $$ = ast_new(N_BLOQUE, NULL, @$.first_line, @$.first_column); }
    | bloque_f sentencia_func {
          ast_add_child($1, $2);
          $$ = $1;
      }
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
    | '(' expresion ')'          { $$ = $2; }
    | NOT expresion {
          $$ = ast_new(N_NEGACION, strdup("!"), @1.first_line, @1.first_column);
          ast_add_child($$, $2);
      }
    | expresion OR expresion  { $$ = make_binop(N_LOGICA, "||", @2.first_line, @2.first_column, $1, $3); }
    | expresion AND expresion { $$ = make_binop(N_LOGICA, "&&", @2.first_line, @2.first_column, $1, $3); }
    | expresion EQ expresion  { $$ = make_binop(N_COMPARACION, "==", @2.first_line, @2.first_column, $1, $3); }
    | expresion NE expresion  { $$ = make_binop(N_COMPARACION, "!=", @2.first_line, @2.first_column, $1, $3); }
    | expresion LT expresion  { $$ = make_binop(N_COMPARACION, "<", @2.first_line, @2.first_column, $1, $3); }
    | expresion GT expresion  { $$ = make_binop(N_COMPARACION, ">", @2.first_line, @2.first_column, $1, $3); }
    | expresion LE expresion  { $$ = make_binop(N_COMPARACION, "<=", @2.first_line, @2.first_column, $1, $3); }
    | expresion GE expresion  { $$ = make_binop(N_COMPARACION, ">=", @2.first_line, @2.first_column, $1, $3); }
    ;

literal
    : LIT_NUMERITO  { $$ = ast_new(N_NUMERO, $1, @1.first_line, @1.first_column); }
    | LIT_FLOTADITO { $$ = ast_new(N_DECIMAL, $1, @1.first_line, @1.first_column); }
    | LIT_FRASE     { $$ = ast_new(N_FRASE, $1, @1.first_line, @1.first_column); }
    | LIT_VIBRA     { $$ = ast_new(N_VIBRA, $1, @1.first_line, @1.first_column); }
    ;

%%

/* Nombre legible de un simbolo: los alias entre comillas (como "uwu") se muestran sin ellas */
static void symbol_text(yysymbol_kind_t k, char *dst, size_t size)
{
    const char *name = yysymbol_name(k);
    size_t n = strlen(name);
    if (n >= 2 && name[0] == '"' && name[n - 1] == '"') {
        name++;
        n -= 2;
    }
    if (n >= size)
        n = size - 1;
    memcpy(dst, name, n);
    dst[n] = '\0';
}

static int is_operator(yysymbol_kind_t k)
{
    return k == YYSYMBOL_OR || k == YYSYMBOL_AND || k == YYSYMBOL_EQ || k == YYSYMBOL_NE
        || k == YYSYMBOL_LT || k == YYSYMBOL_GT || k == YYSYMBOL_LE || k == YYSYMBOL_GE;
}

/* Mensaje de error sintactico: "se encontró X, se esperaba A o B".
   Los operadores se agrupan en "un operador" y, si aun asi hay demasiadas opciones
   (mas de 4), no se lista lo esperado porque dejaria de ser util. */
int yyreport_syntax_error(const yypcontext_t *ctx)
{
    enum { MAX = 32 };
    yysymbol_kind_t expected[MAX];
    int n = yypcontext_expected_tokens(ctx, expected, MAX);
    if (n < 0)
        return n;

    char found[128] = "";
    yysymbol_kind_t un = yypcontext_token(ctx);
    if (un != YYSYMBOL_YYEMPTY)
        symbol_text(un, found, sizeof found);

    char msg[512];
    int len = snprintf(msg, sizeof msg, "se encontró %s", found);

    int count = 0, has_op = 0;
    char names[MAX][128];
    for (int i = 0; i < n; i++) {
        if (is_operator(expected[i]))
            has_op = 1;
        else
            symbol_text(expected[i], names[count++], sizeof names[0]);
    }
    if (count + has_op >= 1 && count + has_op <= 4) {
        len += snprintf(msg + len, sizeof msg - len, ", se esperaba ");
        for (int i = 0; i < count && len < (int)sizeof msg; i++)
            len += snprintf(msg + len, sizeof msg - len, "%s%s", i ? " o " : "", names[i]);
        if (has_op && len < (int)sizeof msg)
            snprintf(msg + len, sizeof msg - len, "%sun operador", count ? " o " : "");
    }

    report_error("error sintáctico", yylloc.first_line, yylloc.first_column,
                 "%s (conjuro mal recitado)", msg);
    return 0;
}

/* Bison solo llama a yyerror para errores que no son de sintaxis (por ejemplo, falta de memoria) */
void yyerror(const char *msg)
{
    report_error("error sintáctico", yylloc.first_line, yylloc.first_column, "%s", msg);
}
