#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"
#include "table.h"

/* Ambito de un simbolo: SCOPE_GLOBAL, o el indice (en la tabla) de la funcion a la que pertenece */
#define SCOPE_GLOBAL (-1)

/* Valor de una variable durante la ejecucion */
typedef struct {
    CType type;
    int defined;        /* 0 = la variable aun no tiene valor */
    long long i;        /* numerito */
    double d;           /* flotadito */
    char *s;            /* frase (sin comillas) */
    int b;              /* vibra: 1 = chi, 0 = nio */
} Value;

/* Categoria del simbolo; son bits para poder buscar varias a la vez */
typedef enum {
    SYM_VAR = 1,        /* variable (global o local de una funcion) */
    SYM_PARAM = 2,      /* parametro de una funcion */
    SYM_FUNC = 4        /* funcion (siempre en el ambito global) */
} SymKind;

typedef struct {
    char *name;
    SymKind kind;
    CType type;         /* variable o parametro: su tipo. Funcion: tipo que devuelve (TY_NADA si no devuelve) */
    int scope;          /* SCOPE_GLOBAL o indice de la funcion duena */
    int block;          /* 0 = nivel principal del ambito; otro numero = bloque de un 'shi'/'sino' */
    int block_line;     /* linea del 'shi' o 'sino' que abrio el bloque (0 si block == 0) */
    int line, col;      /* donde se declaro */
    int initialized;    /* analisis estatico: la variable ya tiene valor en este punto */
    int callable;       /* funciones: 1 cuando termino de analizarse su cuerpo (antes no puede llamarse) */
    int nparams;        /* funciones: cantidad de parametros */
    CType *param_types; /* funciones: tipo de cada parametro, en orden */
    Value value;        /* memoria de ejecucion (solo variables globales) */
} Symbol;

/* Tabla de simbolos. Arreglo dinamico en orden de declaracion; cada simbolo guarda su ambito. */
typedef struct {
    Symbol *items;
    int n, cap;
} SymTab;

void symtab_init(SymTab *st);

/* Agrega un simbolo y devuelve su indice. Los punteros obtenidos con symtab_at
   dejan de ser validos despues de una insercion. */
int symtab_insert(SymTab *st, const char *name, SymKind kind, CType type, int scope,
                  int line, int col, int initialized);

/* Busca 'name' solo dentro de 'scope' y solo entre las categorias indicadas en 'kinds'
   (por ejemplo SYM_VAR | SYM_PARAM). Devuelve el indice o -1. */
int symtab_find(const SymTab *st, const char *name, int scope, int kinds);

Symbol *symtab_at(SymTab *st, int index);

/* Elimina todos los simbolos desde la posicion n en adelante */
void symtab_truncate(SymTab *st, int n);

void symtab_free(SymTab *st);

/* Cadena lista para mostrar: 20, 1.75, "Ana", chi, (sin valor). Hay que liberarla con free. */
char *value_repr(const Value *v);

/* Igual que value_repr pero las frases van sin comillas (es lo que imprime 'muestro') */
char *value_text(const Value *v);

/* 'executed' indica si el programa se ejecuto (si no, la columna de valor muestra "-") */
Table *symtab_to_table(const SymTab *st, int executed);

#endif
