#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symtab.h"

/* Recorre el AST en orden, llena la tabla de simbolos, anota el tipo de cada nodo
   y reporta todos los errores semanticos encontrados. Devuelve la cantidad de errores. */
int semantic_analyze(Node *root, SymTab *st);

#endif
