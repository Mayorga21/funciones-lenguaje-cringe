#ifndef INTERP_H
#define INTERP_H

#include <stdio.h>
#include "ast.h"
#include "symtab.h"

/* Ejecuta el programa ya validado.
   - Cada llamada a una funcion tiene su propia memoria (marco) con sus parametros y variables locales.
   - Las variables globales quedan guardadas en la tabla de simbolos (campo value).
   - En 'out' se escribe una linea de traza por sentencia ejecutada.
   - Devuelve (con malloc, hay que liberarla con free) todo lo que imprimio 'muestro'. */
char *interp_run(const Node *root, SymTab *st, FILE *out);

#endif
