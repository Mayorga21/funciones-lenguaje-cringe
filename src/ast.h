#ifndef AST_H
#define AST_H

#include <stdio.h>
#include "table.h"

/* Tipos de dato de Cringe.
   TY_NADA    = tipo de retorno de una funcion que no devuelve valor
   TY_NINGUNO = el nodo no tiene tipo
   TY_ERROR   = el tipo no se pudo determinar (hubo un error antes) */
typedef enum {
    TY_NUMERITO,
    TY_FLOTADITO,
    TY_FRASE,
    TY_VIBRA,
    TY_NADA,
    TY_NINGUNO,
    TY_ERROR
} CType;

/* Tipos de nodo del AST */
typedef enum {
    N_PROGRAMA,
    N_DECLARACION,
    N_ASIGNACION,
    N_TIPO,
    N_NOMBRE,
    N_NUMERO,
    N_DECIMAL,
    N_FRASE,
    N_VIBRA,
    N_FUNCION,      /* hijos: Tipo (retorno), Nombre, Parametros, Cuerpo */
    N_PARAMETROS,   /* hijos: Parametro* */
    N_PARAMETRO,    /* hijos: Tipo, Nombre */
    N_CUERPO,       /* hijos: sentencias de la funcion */
    N_LLAMADA,      /* hijos: Nombre, Argumentos */
    N_ARGUMENTOS,   /* hijos: expresiones */
    N_RETORNO,      /* hijos: expresion (opcional) */
    N_MUESTRO       /* hijos: expresion */
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    int id;               /* numero de nodo en recorrido preorden (lo asigna ast_assign_ids) */
    char *text;           /* lexema: nombre, valor o tipo; NULL en nodos internos */
    int line, col;
    CType type;           /* lo llena el analisis semantico */
    CType convert_to;     /* conversion implicita aplicada al valor (TY_NINGUNO si no hay) */
    Node *parent;
    Node **children;
    int nchildren, cap;
};

/* Crea un nodo. Toma posesion de 'text' (debe venir de malloc/strdup o ser NULL). */
Node *ast_new(NodeKind kind, char *text, int line, int col);
void ast_add_child(Node *parent, Node *child);
void ast_assign_ids(Node *root);
void ast_print_tree(FILE *out, const Node *root);
Table *ast_to_table(const Node *root);
void ast_free(Node *root);

const char *node_kind_name(NodeKind k);
const char *ctype_name(CType t);
CType ctype_from_name(const char *name);

#endif
