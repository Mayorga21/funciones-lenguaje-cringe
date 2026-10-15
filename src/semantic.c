#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "errors.h"

/* Analisis semantico de Cringe.
   Recorre el AST en orden, llena la tabla de simbolos y anota el tipo de cada nodo.
   Ambitos: el programa tiene un ambito global; cada funcion tiene el suyo (sus parametros
   y variables locales). Dentro de una funcion solo se ven esos simbolos, no los globales. */

static int nerrors;

#define SEM_ERROR(node, ...) \
    do { \
        nerrors++; \
        report_error("error semántico", (node)->line, (node)->col, __VA_ARGS__); \
    } while (0)

/* Un valor de tipo 'src' cabe en un destino de tipo 'dest' si son iguales
   o si es la conversion implicita numerito a flotadito. */
static int compatible(CType dest, CType src)
{
    return dest == src || (dest == TY_FLOTADITO && src == TY_NUMERITO);
}

static const char *kind_phrase(SymKind k)
{
    return k == SYM_FUNC ? "un hechizo" : k == SYM_PARAM ? "un ingrediente" : "una variable";
}

/* 'what' es lo que se intenta declarar; 'prev' es el simbolo que ya tenia ese nombre */
static void report_taken(Node *at, SymKind what, const Symbol *prev)
{
    if (what != prev->kind) {
        SEM_ERROR(at, "el nombre '%s' ya fue invocado como %s (línea %d, columna %d)",
                  prev->name, kind_phrase(prev->kind), prev->line, prev->col);
    } else if (what == SYM_PARAM) {
        SEM_ERROR(at, "el ingrediente '%s' está repetido (ya fue declarado en la línea %d, columna %d)",
                  prev->name, prev->line, prev->col);
    } else if (what == SYM_FUNC) {
        SEM_ERROR(at, "el hechizo '%s' ya fue declarado en la línea %d, columna %d",
                  prev->name, prev->line, prev->col);
    } else {
        SEM_ERROR(at, "la variable '%s' ya fue invocada en la línea %d, columna %d",
                  prev->name, prev->line, prev->col);
    }
}

/* Un nombre usado como variable no se encontro en el ambito actual */
static void report_undeclared(Node *n, SymTab *st, int scope)
{
    const char *name = n->text;

    if (symtab_find(st, name, SCOPE_GLOBAL, SYM_FUNC) >= 0)
        SEM_ERROR(n, "'%s' es un hechizo, no una variable (para lanzarlo escribe %s(...) uwu)",
                  name, name);
    else if (scope != SCOPE_GLOBAL && symtab_find(st, name, SCOPE_GLOBAL, SYM_VAR) >= 0)
        SEM_ERROR(n, "la variable '%s' no fue invocada dentro del hechizo '%s' "
                     "(los hechizos solo pueden usar sus ingredientes y variables locales)",
                  name, st->items[scope].name);
    else
        SEM_ERROR(n, "la variable '%s' no fue invocada", name);
}

static void check_call(Node *call, SymTab *st, int scope);

/* Determina el tipo de una expresion: literal, nombre de variable o llamada a funcion */
static void check_expr(Node *e, SymTab *st, int scope)
{
    switch (e->kind) {
    case N_NUMERO:  e->type = TY_NUMERITO;  break;
    case N_DECIMAL: e->type = TY_FLOTADITO; break;
    case N_FRASE:   e->type = TY_FRASE;     break;
    case N_VIBRA:   e->type = TY_VIBRA;     break;
    case N_NOMBRE: {
        int i = symtab_find(st, e->text, scope, SYM_VAR | SYM_PARAM);
        if (i < 0) {
            report_undeclared(e, st, scope);
            e->type = TY_ERROR;
        } else {
            Symbol *s = symtab_at(st, i);
            if (!s->initialized)
                SEM_ERROR(e, "la variable '%s' se usa sin tener un valor asignado "
                             "(fue invocada sin equipar en la línea %d)", e->text, s->line);
            e->type = s->type;
        }
        break;
    }
    case N_LLAMADA:
        check_call(e, st, scope);
        break;
    default:
        e->type = TY_ERROR;
        break;
    }
}

/* Igual que check_expr, pero la expresion tiene que producir un valor
   (una funcion de tipo 'nada' no sirve). */
static void check_value(Node *e, SymTab *st, int scope)
{
    check_expr(e, st, scope);
    if (e->type == TY_NADA) {
        SEM_ERROR(e, "el hechizo '%s' no devuelve valor (es de tipo 'vacio') y no puede usarse como valor",
                  e->children[0]->text);
        e->type = TY_ERROR;
    }
}

/* Verifica que 'expr' se pueda guardar en una variable de tipo 'dest' */
static void check_assignable(CType dest, Node *expr, const char *name)
{
    if (dest == TY_ERROR || expr->type == TY_ERROR)
        return;

    if (!compatible(dest, expr->type)) {
        SEM_ERROR(expr, "tipos incompatibles: no se puede guardar un valor '%s' "
                        "en la variable '%s' de tipo '%s'",
                  ctype_name(expr->type), name, ctype_name(dest));
    } else if (dest != expr->type) {
        expr->convert_to = dest;    /* conversion implicita numerito a flotadito */
    }
}

/* Llamada a funcion: la funcion debe estar declarada antes, con la cantidad y los tipos
   de argumentos correctos. El tipo de la llamada es el tipo que devuelve la funcion. */
static void check_call(Node *call, SymTab *st, int scope)
{
    Node *nombre = call->children[0];
    Node *args = call->children[1];

    /* Los argumentos se revisan siempre, para reportar sus propios errores */
    for (int i = 0; i < args->nchildren; i++)
        check_value(args->children[i], st, scope);

    int fi = symtab_find(st, nombre->text, SCOPE_GLOBAL, SYM_FUNC);
    if (fi < 0) {
        if (symtab_find(st, nombre->text, scope, SYM_VAR | SYM_PARAM) >= 0)
            SEM_ERROR(nombre, "'%s' es una variable, no un hechizo", nombre->text);
        else
            SEM_ERROR(nombre, "el hechizo '%s' no fue declarado (debe declararse antes de lanzarlo)",
                      nombre->text);
        call->type = nombre->type = TY_ERROR;
        return;
    }

    const Symbol *f = symtab_at(st, fi);
    if (!f->callable) {
        /* solo la funcion que se esta analizando en este momento aun no es llamable */
        SEM_ERROR(nombre, "el hechizo '%s' no puede lanzarse a sí mismo: Cringe aún no tiene "
                          "condicionales y sería un loop infinito de isekai", nombre->text);
        call->type = nombre->type = TY_ERROR;
        return;
    }

    call->type = nombre->type = f->type;

    if (args->nchildren != f->nparams) {
        SEM_ERROR(nombre, "el hechizo '%s' espera %d argumento(s) pero recibió %d",
                  nombre->text, f->nparams, args->nchildren);
        return;
    }

    for (int i = 0; i < args->nchildren; i++) {
        Node *a = args->children[i];
        if (a->type == TY_ERROR)
            continue;
        if (!compatible(f->param_types[i], a->type))
            SEM_ERROR(a, "argumento %d de '%s': se esperaba '%s' pero se recibió '%s'",
                      i + 1, nombre->text, ctype_name(f->param_types[i]), ctype_name(a->type));
        else if (f->param_types[i] != a->type)
            a->convert_to = f->param_types[i];
    }
}

static void check_declaracion(Node *d, SymTab *st, int scope)
{
    Node *tipo = d->children[0];
    Node *nombre = d->children[1];
    Node *init = d->nchildren > 2 ? d->children[2] : NULL;

    CType t = ctype_from_name(tipo->text);
    tipo->type = t;
    nombre->type = t;

    /* En el ambito global los nombres de funciones tambien cuentan */
    int kinds = SYM_VAR | SYM_PARAM | (scope == SCOPE_GLOBAL ? SYM_FUNC : 0);
    int prev = symtab_find(st, nombre->text, scope, kinds);
    if (prev >= 0)
        report_taken(nombre, SYM_VAR, symtab_at(st, prev));

    /* La expresion se revisa ANTES de insertar el simbolo: una variable
       no puede usarse en su propio valor inicial. */
    if (init) {
        check_value(init, st, scope);
        check_assignable(t, init, nombre->text);
    }

    if (prev < 0)
        symtab_insert(st, nombre->text, SYM_VAR, t, scope, nombre->line, nombre->col, init != NULL);
}

static void check_asignacion(Node *a, SymTab *st, int scope)
{
    Node *nombre = a->children[0];
    Node *expr = a->children[1];

    int i = symtab_find(st, nombre->text, scope, SYM_VAR | SYM_PARAM);
    if (i < 0) {
        report_undeclared(nombre, st, scope);
        nombre->type = TY_ERROR;
    } else {
        nombre->type = symtab_at(st, i)->type;
    }

    check_value(expr, st, scope);

    if (i >= 0) {
        Symbol *s = symtab_at(st, i);
        check_assignable(s->type, expr, nombre->text);
        s->initialized = 1;     /* aunque haya error de tipo, se evita un segundo error en cascada */
    }
}

static void check_muestro(Node *m, SymTab *st, int scope)
{
    check_value(m->children[0], st, scope);
}

/* 'devuelvo' dentro de la funcion cuyo simbolo esta en la posicion 'scope' */
static void check_retorno(Node *r, SymTab *st, int scope)
{
    const Symbol *f = symtab_at(st, scope);
    CType rt = f->type;
    r->type = rt;

    if (r->nchildren == 0) {
        if (rt != TY_NADA)
            SEM_ERROR(r, "el hechizo '%s' debe devolver un valor '%s', pero este 'regreso' no trae ninguno",
                      f->name, ctype_name(rt));
        return;
    }

    Node *e = r->children[0];
    check_value(e, st, scope);

    if (rt == TY_NADA) {
        SEM_ERROR(e, "el hechizo '%s' es de tipo 'vacio' y no puede devolver un valor", f->name);
    } else if (e->type != TY_ERROR) {
        if (!compatible(rt, e->type))
            SEM_ERROR(e, "tipos incompatibles: el hechizo '%s' devuelve '%s' pero se intenta devolver un valor '%s'",
                      f->name, ctype_name(rt), ctype_name(e->type));
        else if (rt != e->type)
            e->convert_to = rt;
    }
}

static void check_funcion(Node *f, SymTab *st)
{
    Node *tipo = f->children[0];
    Node *nombre = f->children[1];
    Node *params = f->children[2];
    Node *cuerpo = f->children[3];

    CType rt = ctype_from_name(tipo->text);
    tipo->type = nombre->type = f->type = rt;

    int prev = symtab_find(st, nombre->text, SCOPE_GLOBAL, SYM_VAR | SYM_FUNC);
    if (prev >= 0)
        report_taken(nombre, SYM_FUNC, symtab_at(st, prev));

    /* La funcion se registra antes de revisar su cuerpo (sus parametros y locales
       cuelgan de ella) pero no es llamable hasta terminar: asi no puede llamarse a si misma. */
    int fi = symtab_insert(st, nombre->text, SYM_FUNC, rt, SCOPE_GLOBAL,
                           nombre->line, nombre->col, 0);

    CType *types = malloc(sizeof(CType) * (params->nchildren ? params->nchildren : 1));
    if (!types) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < params->nchildren; i++) {
        Node *p = params->children[i];
        Node *pt = p->children[0];
        Node *pn = p->children[1];

        CType t = ctype_from_name(pt->text);
        pt->type = pn->type = p->type = t;
        types[i] = t;

        int dup = symtab_find(st, pn->text, fi, SYM_PARAM);
        if (dup >= 0)
            report_taken(pn, SYM_PARAM, symtab_at(st, dup));
        else
            symtab_insert(st, pn->text, SYM_PARAM, t, fi, pn->line, pn->col, 1);
    }

    Symbol *fs = symtab_at(st, fi);
    fs->nparams = params->nchildren;
    fs->param_types = types;

    int saw_return = 0, reported_unreachable = 0;
    for (int i = 0; i < cuerpo->nchildren; i++) {
        Node *s = cuerpo->children[i];

        /* Sin condicionales, todo lo que sigue a 'devuelvo' nunca se ejecuta */
        if (saw_return && !reported_unreachable) {
            SEM_ERROR(s, "código inalcanzable: el hechizo '%s' ya terminó en un 'regreso' anterior (el héroe ya volvió a su mundo)",
                      nombre->text);
            reported_unreachable = 1;
        }

        switch (s->kind) {
        case N_DECLARACION: check_declaracion(s, st, fi); break;
        case N_ASIGNACION:  check_asignacion(s, st, fi);  break;
        case N_LLAMADA:     check_call(s, st, fi);        break;
        case N_MUESTRO:     check_muestro(s, st, fi);     break;
        case N_RETORNO:
            check_retorno(s, st, fi);
            saw_return = 1;
            break;
        default:
            break;
        }
    }

    if (rt != TY_NADA && !saw_return)
        SEM_ERROR(nombre, "el hechizo '%s' debe devolver un valor '%s' (falta 'regreso')",
                  nombre->text, ctype_name(rt));

    if (prev < 0)
        symtab_at(st, fi)->callable = 1;
    else
        symtab_truncate(st, fi);    /* funcion repetida: se revisó para dar sus errores, pero no se registra */
}

int semantic_analyze(Node *root, SymTab *st)
{
    nerrors = 0;
    for (int i = 0; i < root->nchildren; i++) {
        Node *s = root->children[i];
        switch (s->kind) {
        case N_DECLARACION: check_declaracion(s, st, SCOPE_GLOBAL); break;
        case N_ASIGNACION:  check_asignacion(s, st, SCOPE_GLOBAL);  break;
        case N_FUNCION:     check_funcion(s, st);                   break;
        case N_LLAMADA:     check_call(s, st, SCOPE_GLOBAL);        break;
        case N_MUESTRO:     check_muestro(s, st, SCOPE_GLOBAL);     break;
        default:
            break;
        }
    }
    return nerrors;
}
