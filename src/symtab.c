#include <stdlib.h>
#include <string.h>
#include "symtab.h"

static char *dup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *r = malloc(n);
    if (!r) {
        perror("malloc");
        exit(1);
    }
    memcpy(r, s, n);
    return r;
}

void symtab_init(SymTab *st)
{
    st->items = NULL;
    st->n = 0;
    st->cap = 0;
}

int symtab_insert(SymTab *st, const char *name, SymKind kind, CType type, int scope,
                  int line, int col, int initialized)
{
    if (st->n == st->cap) {
        st->cap = st->cap ? st->cap * 2 : 8;
        st->items = realloc(st->items, sizeof(Symbol) * st->cap);
        if (!st->items) {
            perror("realloc");
            exit(1);
        }
    }
    Symbol *s = &st->items[st->n];
    memset(s, 0, sizeof *s);
    s->name = dup(name);
    s->kind = kind;
    s->type = type;
    s->scope = scope;
    s->line = line;
    s->col = col;
    s->initialized = initialized;
    s->value.type = type;
    return st->n++;
}

int symtab_find(const SymTab *st, const char *name, int scope, int kinds)
{
    for (int i = 0; i < st->n; i++) {
        const Symbol *s = &st->items[i];
        if (s->scope == scope && (s->kind & kinds) && strcmp(s->name, name) == 0)
            return i;
    }
    return -1;
}

Symbol *symtab_at(SymTab *st, int index)
{
    return &st->items[index];
}

static void free_symbol(Symbol *s)
{
    free(s->name);
    free(s->value.s);
    free(s->param_types);
}

void symtab_truncate(SymTab *st, int n)
{
    for (int i = n; i < st->n; i++)
        free_symbol(&st->items[i]);
    if (n < st->n)
        st->n = n;
}

void symtab_free(SymTab *st)
{
    symtab_truncate(st, 0);
    free(st->items);
    st->items = NULL;
    st->n = st->cap = 0;
}

char *value_text(const Value *v)
{
    if (v->defined && v->type == TY_FRASE)
        return dup(v->s);
    return value_repr(v);
}

char *value_repr(const Value *v)
{
    char buf[64];

    if (!v->defined)
        return dup("(sin valor)");

    switch (v->type) {
    case TY_NUMERITO:
        snprintf(buf, sizeof buf, "%lld", v->i);
        return dup(buf);
    case TY_FLOTADITO:
        snprintf(buf, sizeof buf, "%.15g", v->d);
        if (!strpbrk(buf, ".eEn"))      /* 5 se muestra como 5.0 */
            strcat(buf, ".0");
        return dup(buf);
    case TY_FRASE: {
        size_t n = strlen(v->s) + 3;
        char *r = malloc(n);
        if (!r) {
            perror("malloc");
            exit(1);
        }
        snprintf(r, n, "\"%s\"", v->s);
        return r;
    }
    case TY_VIBRA:
        return dup(v->b ? "chi" : "nio");
    default:
        break;
    }
    return dup("-");
}

/* Firma de una funcion: (numerito, frase) da numerito */
static void build_signature(const Symbol *f, char *out, size_t size)
{
    size_t n = 0;
    n += snprintf(out + n, size - n, "(");
    for (int i = 0; i < f->nparams && n < size; i++)
        n += snprintf(out + n, size - n, "%s%s", i ? ", " : "", ctype_name(f->param_types[i]));
    if (n < size)
        snprintf(out + n, size - n, ") lanza %s", ctype_name(f->type));
}

Table *symtab_to_table(const SymTab *st, int executed)
{
    Table *t = table_new(10, "#", "Nombre", "Categoría", "Tipo", "Ámbito", "Línea",
                         "Columna", "Inicializada", "Valor final", "Firma");

    for (int i = 0; i < st->n; i++) {
        const Symbol *s = &st->items[i];
        char num[16], line[16], col[16], firma[256] = "-";
        snprintf(num, sizeof num, "%d", i + 1);
        snprintf(line, sizeof line, "%d", s->line);
        snprintf(col, sizeof col, "%d", s->col);

        const char *cat = s->kind == SYM_FUNC ? "hechizo"
                        : s->kind == SYM_PARAM ? "ingrediente" : "variable";
        char scope[160];
        const char *base = s->scope == SCOPE_GLOBAL ? "global" : st->items[s->scope].name;
        if (s->block)
            snprintf(scope, sizeof scope, "%s (bloque línea %d)", base, s->block_line);
        else
            snprintf(scope, sizeof scope, "%s", base);
        const char *init = s->kind == SYM_FUNC ? "-" : (s->initialized ? "si" : "no");

        /* Las variables locales, los parametros y las variables de un bloque solo existen
           mientras dura la llamada o el bloque, por eso no tienen valor final. */
        char *val;
        if (s->kind == SYM_FUNC || s->scope != SCOPE_GLOBAL || s->block || !executed)
            val = dup("-");
        else
            val = value_repr(&s->value);

        if (s->kind == SYM_FUNC)
            build_signature(s, firma, sizeof firma);

        table_add_row(t, num, s->name, cat, ctype_name(s->type), scope, line, col,
                      init, val, firma);
        free(val);
    }
    return t;
}
