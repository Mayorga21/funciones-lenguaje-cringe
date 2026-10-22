#include <stdlib.h>
#include <string.h>
#include "ast.h"

const char *node_kind_name(NodeKind k)
{
    switch (k) {
    case N_PROGRAMA:    return "Programa";
    case N_DECLARACION: return "Declaracion";
    case N_ASIGNACION:  return "Asignacion";
    case N_TIPO:        return "Tipo";
    case N_NOMBRE:      return "Nombre";
    case N_NUMERO:      return "Numero";
    case N_DECIMAL:     return "Decimal";
    case N_FRASE:       return "Frase";
    case N_VIBRA:       return "Vibra";
    case N_FUNCION:     return "Funcion";
    case N_PARAMETROS:  return "Parametros";
    case N_PARAMETRO:   return "Parametro";
    case N_CUERPO:      return "Cuerpo";
    case N_LLAMADA:     return "Llamada";
    case N_ARGUMENTOS:  return "Argumentos";
    case N_RETORNO:     return "Retorno";
    case N_MUESTRO:     return "Muestro";
    case N_SI:          return "Si";
    case N_BLOQUE:      return "Bloque";
    case N_SINO:        return "Sino";
    case N_COMPARACION: return "Comparacion";
    case N_LOGICA:      return "Logica";
    case N_NEGACION:    return "Negacion";
    }
    return "?";
}

const char *ctype_name(CType t)
{
    switch (t) {
    case TY_NUMERITO:  return "numerito";
    case TY_FLOTADITO: return "flotadito";
    case TY_FRASE:     return "pergamino";
    case TY_VIBRA:     return "vibra";
    case TY_NADA:      return "vacio";
    case TY_ERROR:     return "error";
    case TY_NINGUNO:   break;
    }
    return "-";
}

CType ctype_from_name(const char *name)
{
    if (strcmp(name, "numerito") == 0)  return TY_NUMERITO;
    if (strcmp(name, "flotadito") == 0) return TY_FLOTADITO;
    if (strcmp(name, "pergamino") == 0)     return TY_FRASE;
    if (strcmp(name, "vibra") == 0)     return TY_VIBRA;
    if (strcmp(name, "vacio") == 0)      return TY_NADA;
    return TY_ERROR;
}

Node *ast_new(NodeKind kind, char *text, int line, int col)
{
    Node *n = calloc(1, sizeof *n);
    if (!n) {
        perror("calloc");
        exit(1);
    }
    n->kind = kind;
    n->text = text;
    n->line = line;
    n->col = col;
    n->type = TY_NINGUNO;
    n->convert_to = TY_NINGUNO;
    return n;
}

void ast_add_child(Node *parent, Node *child)
{
    if (parent->nchildren == parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->children = realloc(parent->children, sizeof(Node *) * parent->cap);
        if (!parent->children) {
            perror("realloc");
            exit(1);
        }
    }
    parent->children[parent->nchildren++] = child;
    child->parent = parent;
}

static int next_id;

static void assign_rec(Node *n)
{
    n->id = next_id++;
    for (int i = 0; i < n->nchildren; i++)
        assign_rec(n->children[i]);
}

void ast_assign_ids(Node *root)
{
    next_id = 1;
    assign_rec(root);
}

/* Etiqueta del nodo: Nombre(edad), Numero(20), Frase("Ana"), Declaracion... */
static void print_label(FILE *out, const Node *n)
{
    fputs(node_kind_name(n->kind), out);
    if (!n->text)
        return;
    if (n->kind == N_FRASE)
        fprintf(out, "(\"%s\")", n->text);
    else
        fprintf(out, "(%s)", n->text);
}

static void print_rec(FILE *out, const Node *n, int depth)
{
    for (int i = 0; i < depth; i++)
        fputs("  ", out);
    print_label(out, n);
    fputc('\n', out);
    for (int i = 0; i < n->nchildren; i++)
        print_rec(out, n->children[i], depth + 1);
}

void ast_print_tree(FILE *out, const Node *root)
{
    print_rec(out, root, 0);
}

static void table_rec(Table *t, const Node *n)
{
    char id[16], parent[16], line[16], col[16];
    char value[512], hijos[2048], conv[64];

    snprintf(id, sizeof id, "%d", n->id);
    snprintf(line, sizeof line, "%d", n->line);
    snprintf(col, sizeof col, "%d", n->col);

    if (n->parent)
        snprintf(parent, sizeof parent, "%d", n->parent->id);
    else
        snprintf(parent, sizeof parent, "-");

    if (!n->text)
        snprintf(value, sizeof value, "-");
    else if (n->kind == N_FRASE)
        snprintf(value, sizeof value, "\"%s\"", n->text);
    else
        snprintf(value, sizeof value, "%s", n->text);

    hijos[0] = '\0';
    for (int i = 0; i < n->nchildren; i++) {
        char tmp[16];
        snprintf(tmp, sizeof tmp, "%s%d", i ? "," : "", n->children[i]->id);
        strncat(hijos, tmp, sizeof hijos - strlen(hijos) - 1);
    }
    if (!hijos[0])
        snprintf(hijos, sizeof hijos, "-");

    if (n->convert_to != TY_NINGUNO)
        snprintf(conv, sizeof conv, "%s a %s", ctype_name(n->type), ctype_name(n->convert_to));
    else
        snprintf(conv, sizeof conv, "-");

    table_add_row(t, id, node_kind_name(n->kind), value, ctype_name(n->type),
                  conv, parent, hijos, line, col);

    for (int i = 0; i < n->nchildren; i++)
        table_rec(t, n->children[i]);
}

Table *ast_to_table(const Node *root)
{
    Table *t = table_new(9, "ID", "Nodo", "Valor", "Tipo", "Conversión",
                         "Padre", "Hijos", "Línea", "Columna");
    table_rec(t, root);
    return t;
}

void ast_free(Node *n)
{
    if (!n)
        return;
    for (int i = 0; i < n->nchildren; i++)
        ast_free(n->children[i]);
    free(n->children);
    free(n->text);
    free(n);
}
