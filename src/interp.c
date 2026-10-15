#include <stdlib.h>
#include <string.h>
#include "interp.h"

/* Intérprete de Cringe: recorre el AST y ejecuta las sentencias.
   Memoria: una variable vive en un Slot dentro de un Frame (marco).
   El programa tiene un marco global; cada llamada a funcion crea uno nuevo que se
   destruye al volver. Como las funciones solo ven sus propios simbolos, el marco de
   una llamada nunca toca el del que la llamo.
   Las frases nunca se copian en ejecucion: siempre apuntan al texto del literal en el AST. */

typedef struct {
    const char *name;
    CType type;         /* tipo declarado de la variable */
    Value v;
} Slot;

typedef struct {
    Slot *slots;
    int n, cap;
} Frame;

typedef struct {
    const Node *root;
    FILE *trace;
    char *out;          /* lo que imprimio 'muestro' */
    size_t out_len, out_cap;
    int depth;          /* profundidad de llamadas, solo para sangrar la traza */
} Ctx;

static void fatal(const char *msg, const char *name)
{
    fprintf(stderr, "error interno de ejecución: %s '%s'\n", msg, name);
    exit(1);
}

/* ---------- marcos ---------- */

static int frame_add(Frame *f, const char *name, CType type, Value v)
{
    if (f->n == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 8;
        f->slots = realloc(f->slots, sizeof(Slot) * f->cap);
        if (!f->slots) {
            perror("realloc");
            exit(1);
        }
    }
    f->slots[f->n].name = name;
    f->slots[f->n].type = type;
    f->slots[f->n].v = v;
    return f->n++;
}

static Slot *frame_find(Frame *f, const char *name)
{
    for (int i = 0; i < f->n; i++)
        if (strcmp(f->slots[i].name, name) == 0)
            return &f->slots[i];
    return NULL;
}

static void frame_free(Frame *f)
{
    free(f->slots);
    f->slots = NULL;
    f->n = f->cap = 0;
}

/* ---------- valores ---------- */

static Value literal_value(const Node *e)
{
    Value v;
    memset(&v, 0, sizeof v);
    v.defined = 1;

    switch (e->kind) {
    case N_NUMERO:
        v.type = TY_NUMERITO;
        v.i = strtoll(e->text, NULL, 10);
        break;
    case N_DECIMAL:
        v.type = TY_FLOTADITO;
        v.d = strtod(e->text, NULL);
        break;
    case N_FRASE:
        v.type = TY_FRASE;
        v.s = e->text;
        break;
    case N_VIBRA:
        v.type = TY_VIBRA;
        v.b = strcmp(e->text, "chi") == 0;
        break;
    default:
        break;
    }
    return v;
}

/* Conversion implicita numerito a flotadito; cualquier otro caso queda igual */
static Value convert(Value v, CType dest)
{
    if (dest == TY_FLOTADITO && v.type == TY_NUMERITO) {
        v.d = (double)v.i;
        v.type = TY_FLOTADITO;
    }
    return v;
}

/* ---------- salida ---------- */

static void indent(const Ctx *c)
{
    for (int i = 0; i < c->depth * 2; i++)
        fputc(' ', c->trace);
}

static void out_append(Ctx *c, const char *s)
{
    size_t n = strlen(s);
    if (c->out_len + n + 1 > c->out_cap) {
        while (c->out_len + n + 1 > c->out_cap)
            c->out_cap *= 2;
        c->out = realloc(c->out, c->out_cap);
        if (!c->out) {
            perror("realloc");
            exit(1);
        }
    }
    memcpy(c->out + c->out_len, s, n + 1);
    c->out_len += n;
}

/* línea N: nombre (tipo) <- valor [evolución implícita ...] */
static void trace_assign(const Ctx *c, int line, const Slot *s, const Node *expr)
{
    char *val = value_repr(&s->v);
    indent(c);
    fprintf(c->trace, "línea %d: %s (%s) <- %s", line, s->name, ctype_name(s->type), val);
    free(val);
    if (expr->convert_to != TY_NINGUNO)
        fprintf(c->trace, "  [evolución implícita de %s a %s]",
                ctype_name(expr->type), ctype_name(expr->convert_to));
    fputc('\n', c->trace);
}

/* ---------- ejecucion ---------- */

static const Node *find_function(const Node *root, const char *name)
{
    for (int i = 0; i < root->nchildren; i++) {
        const Node *n = root->children[i];
        if (n->kind == N_FUNCION && strcmp(n->children[1]->text, name) == 0)
            return n;
    }
    return NULL;
}

static Value call_function(Ctx *c, Frame *caller, const Node *call);

static Value eval(Ctx *c, Frame *fr, const Node *e)
{
    switch (e->kind) {
    case N_NOMBRE: {
        Slot *s = frame_find(fr, e->text);
        if (!s || !s->v.defined)
            fatal("variable sin valor", e->text);
        return s->v;
    }
    case N_LLAMADA:
        return call_function(c, fr, e);
    default:
        return literal_value(e);
    }
}

static void exec_stmt(Ctx *c, Frame *fr, const Node *s)
{
    switch (s->kind) {
    case N_DECLARACION: {
        CType t = ctype_from_name(s->children[0]->text);
        const char *name = s->children[1]->text;

        if (s->nchildren > 2) {
            const Node *init = s->children[2];
            Value v = convert(eval(c, fr, init), t);
            int i = frame_add(fr, name, t, v);
            trace_assign(c, s->line, &fr->slots[i], init);
        } else {
            Value v;
            memset(&v, 0, sizeof v);
            v.type = t;
            frame_add(fr, name, t, v);
            indent(c);
            fprintf(c->trace, "línea %d: %s (%s) invocada sin equipar\n",
                    s->line, name, ctype_name(t));
        }
        break;
    }
    case N_ASIGNACION: {
        const Node *expr = s->children[1];
        Value v = eval(c, fr, expr);
        Slot *slot = frame_find(fr, s->children[0]->text);
        if (!slot)
            fatal("variable inexistente", s->children[0]->text);
        slot->v = convert(v, slot->type);
        trace_assign(c, s->line, slot, expr);
        break;
    }
    case N_LLAMADA:
        (void)call_function(c, fr, s);      /* el valor devuelto, si lo hay, se descarta */
        break;
    case N_MUESTRO: {
        Value v = eval(c, fr, s->children[0]);
        char *repr = value_repr(&v);
        char *text = value_text(&v);
        indent(c);
        fprintf(c->trace, "línea %d: narro %s\n", s->line, repr);
        out_append(c, text);
        out_append(c, "\n");
        free(repr);
        free(text);
        break;
    }
    default:        /* funciones: se ejecutan solo cuando las llaman */
        break;
    }
}

static Value call_function(Ctx *c, Frame *caller, const Node *call)
{
    const char *name = call->children[0]->text;
    const Node *args = call->children[1];
    const Node *fn = find_function(c->root, name);
    if (!fn)
        fatal("función inexistente", name);
    if (c->depth > 200)
        fatal("demasiadas llamadas anidadas en", name);

    CType rt = ctype_from_name(fn->children[0]->text);
    const Node *params = fn->children[2];
    const Node *body = fn->children[3];

    /* Los argumentos se evaluan en el marco de quien llama */
    Value *vals = calloc(args->nchildren ? args->nchildren : 1, sizeof(Value));
    if (!vals) {
        perror("calloc");
        exit(1);
    }
    for (int i = 0; i < args->nchildren; i++)
        vals[i] = eval(c, caller, args->children[i]);

    indent(c);
    fprintf(c->trace, "línea %d: lanzo %s(", call->line, name);
    for (int i = 0; i < args->nchildren; i++) {
        char *r = value_repr(&vals[i]);
        fprintf(c->trace, "%s%s", i ? ", " : "", r);
        free(r);
    }
    fprintf(c->trace, ")\n");

    /* Marco nuevo: los parametros son sus primeras variables */
    Frame fr = {0};
    for (int i = 0; i < params->nchildren; i++) {
        const Node *p = params->children[i];
        CType pt = ctype_from_name(p->children[0]->text);
        frame_add(&fr, p->children[1]->text, pt, convert(vals[i], pt));
    }
    free(vals);

    Value ret;
    memset(&ret, 0, sizeof ret);
    ret.type = rt;

    c->depth++;
    for (int i = 0; i < body->nchildren; i++) {
        const Node *s = body->children[i];

        if (s->kind == N_RETORNO) {
            indent(c);
            if (s->nchildren > 0) {
                const Node *e = s->children[0];
                ret = convert(eval(c, &fr, e), rt);
                char *r = value_repr(&ret);
                fprintf(c->trace, "línea %d: regreso %s", s->line, r);
                free(r);
                if (e->convert_to != TY_NINGUNO)
                    fprintf(c->trace, "  [evolución implícita de %s a %s]",
                            ctype_name(e->type), ctype_name(e->convert_to));
                fputc('\n', c->trace);
            } else {
                fprintf(c->trace, "línea %d: regreso\n", s->line);
            }
            break;
        }
        exec_stmt(c, &fr, s);
    }
    c->depth--;

    frame_free(&fr);
    return ret;
}

char *interp_run(const Node *root, SymTab *st, FILE *out)
{
    Ctx c;
    memset(&c, 0, sizeof c);
    c.root = root;
    c.trace = out;
    c.out_cap = 256;
    c.out = malloc(c.out_cap);
    if (!c.out) {
        perror("malloc");
        exit(1);
    }
    c.out[0] = '\0';

    Frame global = {0};
    for (int i = 0; i < root->nchildren; i++)
        exec_stmt(&c, &global, root->children[i]);

    /* Los valores finales de las variables globales pasan a la tabla de simbolos */
    for (int i = 0; i < global.n; i++) {
        int idx = symtab_find(st, global.slots[i].name, SCOPE_GLOBAL, SYM_VAR);
        if (idx < 0)
            continue;
        Symbol *sym = symtab_at(st, idx);
        free(sym->value.s);
        sym->value = global.slots[i].v;
        sym->value.s = global.slots[i].v.s ? strdup(global.slots[i].v.s) : NULL;
    }
    frame_free(&global);

    return c.out;
}
