#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "errors.h"

/* Analisis semantico de Cringe.
   Recorre el AST en orden, llena la tabla de simbolos y anota el tipo de cada nodo.
   Ambitos: el programa tiene un ambito global; cada funcion tiene el suyo (sus parametros
   y variables locales). Dentro de una funcion solo se ven esos simbolos, no los globales.
   Dentro de un ambito, cada bloque de un 'shi' o 'sino' abre un ambito anidado: sus variables
   solo existen hasta la llave de cierre, y no se puede repetir un nombre que ya es visible. */

static int nerrors;

/* Pila de bloques abiertos. active[0] = 0 es siempre el nivel principal del ambito. */
static int *active, *active_line;
static int nactive, cap_active, next_block;

static void push_block(int line)
{
    if (nactive == cap_active) {
        cap_active = cap_active ? cap_active * 2 : 16;
        active = realloc(active, sizeof(int) * cap_active);
        active_line = realloc(active_line, sizeof(int) * cap_active);
        if (!active || !active_line) {
            perror("realloc");
            exit(1);
        }
    }
    active[nactive] = nactive == 0 ? 0 : ++next_block;
    active_line[nactive] = line;
    nactive++;
}

static void pop_block(void)
{
    nactive--;
}

static int in_chain(int block)
{
    for (int i = 0; i < nactive; i++)
        if (active[i] == block)
            return 1;
    return 0;
}

/* Como symtab_find, pero solo ve los simbolos de los bloques que estan abiertos ahora */
static int lookup(const SymTab *st, const char *name, int scope, int kinds)
{
    for (int i = 0; i < st->n; i++) {
        const Symbol *s = &st->items[i];
        if (s->scope == scope && (s->kind & kinds) && in_chain(s->block)
            && strcmp(s->name, name) == 0)
            return i;
    }
    return -1;
}

/* Inserta un simbolo en el bloque que esta abierto en este momento */
static int declare(SymTab *st, const char *name, SymKind kind, CType type, int scope,
                   int line, int col, int initialized)
{
    int i = symtab_insert(st, name, kind, type, scope, line, col, initialized);
    Symbol *s = symtab_at(st, i);
    s->block = active[nactive - 1];
    s->block_line = s->block ? active_line[nactive - 1] : 0;
    return i;
}

static int is_num(CType t)
{
    return t == TY_NUMERITO || t == TY_FLOTADITO;
}

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
static void check_value(Node *e, SymTab *st, int scope);

/* Determina el tipo de una expresion: literal, nombre de variable o llamada a funcion */
static void check_expr(Node *e, SymTab *st, int scope)
{
    switch (e->kind) {
    case N_NUMERO:  e->type = TY_NUMERITO;  break;
    case N_DECIMAL: e->type = TY_FLOTADITO; break;
    case N_FRASE:   e->type = TY_FRASE;     break;
    case N_VIBRA:   e->type = TY_VIBRA;     break;
    case N_NOMBRE: {
        int i = lookup(st, e->text, scope, SYM_VAR | SYM_PARAM);
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
    case N_NEGACION: {
        Node *o = e->children[0];
        check_value(o, st, scope);
        e->type = TY_VIBRA;
        if (o->type != TY_ERROR && o->type != TY_VIBRA)
            SEM_ERROR(e, "el operador '!' necesita un valor 'vibra', pero recibió un valor '%s'",
                      ctype_name(o->type));
        break;
    }
    case N_LOGICA:
        e->type = TY_VIBRA;
        for (int i = 0; i < 2; i++) {
            Node *o = e->children[i];
            check_value(o, st, scope);
            if (o->type != TY_ERROR && o->type != TY_VIBRA)
                SEM_ERROR(o, "el operador '%s' necesita valores 'vibra', pero recibió un valor '%s'",
                          e->text, ctype_name(o->type));
        }
        break;
    case N_COMPARACION: {
        Node *l = e->children[0], *r = e->children[1];
        check_value(l, st, scope);
        check_value(r, st, scope);
        e->type = TY_VIBRA;
        if (l->type == TY_ERROR || r->type == TY_ERROR)
            break;
        int orden = strcmp(e->text, "==") != 0 && strcmp(e->text, "!=") != 0;
        if (is_num(l->type) && is_num(r->type)) {
            /* numerito contra flotadito: el numerito evoluciona a flotadito */
            if (l->type != r->type)
                (l->type == TY_NUMERITO ? l : r)->convert_to = TY_FLOTADITO;
        } else if (l->type == r->type && (l->type == TY_FRASE || l->type == TY_VIBRA)) {
            if (orden)
                SEM_ERROR(e, "el operador '%s' solo compara numeritos y flotaditos, no valores '%s'",
                          e->text, ctype_name(l->type));
        } else {
            SEM_ERROR(e, "no se puede comparar un valor '%s' con un valor '%s'",
                      ctype_name(l->type), ctype_name(r->type));
        }
        break;
    }
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
        if (lookup(st, nombre->text, scope, SYM_VAR | SYM_PARAM) >= 0)
            SEM_ERROR(nombre, "'%s' es una variable, no un hechizo", nombre->text);
        else
            SEM_ERROR(nombre, "el hechizo '%s' no fue declarado (debe declararse antes de lanzarlo)",
                      nombre->text);
        call->type = nombre->type = TY_ERROR;
        return;
    }

    const Symbol *f = symtab_at(st, fi);
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
    int prev = lookup(st, nombre->text, scope, kinds);
    if (prev >= 0)
        report_taken(nombre, SYM_VAR, symtab_at(st, prev));

    /* La expresion se revisa ANTES de insertar el simbolo: una variable
       no puede usarse en su propio valor inicial. */
    if (init) {
        check_value(init, st, scope);
        check_assignable(t, init, nombre->text);
    }

    if (prev < 0)
        declare(st, nombre->text, SYM_VAR, t, scope, nombre->line, nombre->col, init != NULL);
}

static void check_asignacion(Node *a, SymTab *st, int scope)
{
    Node *nombre = a->children[0];
    Node *expr = a->children[1];

    int i = lookup(st, nombre->text, scope, SYM_VAR | SYM_PARAM);
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

static void check_funcion(Node *f, SymTab *st);
static int check_stmts(Node *list, SymTab *st, int scope);

/* El nodo contiene algun 'regreso' en cualquier rama */
static int has_return(const Node *n)
{
    if (n->kind == N_RETORNO)
        return 1;
    for (int i = 0; i < n->nchildren; i++)
        if (has_return(n->children[i]))
            return 1;
    return 0;
}

/* Revisa las sentencias de una rama (shi o sino) dentro de su propio bloque.
   Devuelve 1 si la rama siempre termina en un 'regreso'. */
static int check_branch(Node *rama, SymTab *st, int scope, int line)
{
    push_block(line);
    int returns = check_stmts(rama, st, scope);
    pop_block();
    return returns;
}

/* Copia de la bandera 'initialized' de los primeros n simbolos */
static int *snapshot(const SymTab *st, int n)
{
    int *a = malloc(sizeof(int) * (n ? n : 1));
    if (!a) {
        perror("malloc");
        exit(1);
    }
    for (int i = 0; i < n; i++)
        a[i] = st->items[i].initialized;
    return a;
}

/* shi (condicion) { ... } sino { ... }
   La condicion debe ser 'vibra'. Una variable declarada sin valor queda con valor despues
   del shi solo si TODOS los caminos que no terminan en 'regreso' le asignaron uno.
   Devuelve 1 si ambas ramas existen y siempre terminan en un 'regreso'. */
static int check_si(Node *si, SymTab *st, int scope)
{
    Node *cond = si->children[0];
    Node *entonces = si->children[1];
    Node *otro = si->nchildren > 2 ? si->children[2] : NULL;

    check_value(cond, st, scope);
    if (cond->type != TY_ERROR && cond->type != TY_VIBRA)
        SEM_ERROR(cond, "la condición del 'shi' debe ser de tipo 'vibra' pero es de tipo '%s' "
                        "(el gremio solo acepta chi o nio)", ctype_name(cond->type));

    int n = st->n;      /* simbolos que ya existen: los unicos que sobreviven al bloque */
    int *antes = snapshot(st, n);

    int r1 = check_branch(entonces, st, scope, si->line);
    int *tras_entonces = snapshot(st, n);

    for (int i = 0; i < n; i++)
        st->items[i].initialized = antes[i];

    int r2 = 0;
    int *tras_otro;
    if (otro) {
        r2 = check_branch(otro, st, scope, otro->line);
        tras_otro = snapshot(st, n);
    } else {
        tras_otro = snapshot(st, n);    /* sin sino: no se asigno nada por ese camino */
    }

    for (int i = 0; i < n; i++)
        st->items[i].initialized = (r1 || tras_entonces[i]) && (r2 || tras_otro[i]);

    free(antes);
    free(tras_entonces);
    free(tras_otro);
    return otro && r1 && r2;
}

/* Revisa una lista de sentencias en orden. Devuelve 1 si siempre termina en un 'regreso'. */
static int check_stmts(Node *list, SymTab *st, int scope)
{
    int returned = 0, reported = 0;

    for (int i = 0; i < list->nchildren; i++) {
        Node *s = list->children[i];

        /* Todo lo que sigue a un 'regreso' que siempre se ejecuta nunca se ejecuta */
        if (returned && !reported) {
            SEM_ERROR(s, "código inalcanzable: el hechizo '%s' ya terminó en un 'regreso' anterior (el héroe ya volvió a su mundo)",
                      st->items[scope].name);
            reported = 1;
        }

        switch (s->kind) {
        case N_DECLARACION: check_declaracion(s, st, scope); break;
        case N_ASIGNACION:  check_asignacion(s, st, scope);  break;
        case N_LLAMADA:     check_call(s, st, scope);        break;
        case N_MUESTRO:     check_muestro(s, st, scope);     break;
        case N_FUNCION:     check_funcion(s, st);            break;
        case N_RETORNO:
            check_retorno(s, st, scope);
            returned = 1;
            break;
        case N_SI:
            if (check_si(s, st, scope))
                returned = 1;
            break;
        default:
            break;
        }
    }
    return returned;
}

static void check_funcion(Node *f, SymTab *st)
{
    Node *tipo = f->children[0];
    Node *nombre = f->children[1];
    Node *params = f->children[2];
    Node *cuerpo = f->children[3];

    CType rt = ctype_from_name(tipo->text);
    tipo->type = nombre->type = f->type = rt;

    int prev = lookup(st, nombre->text, SCOPE_GLOBAL, SYM_VAR | SYM_FUNC);
    if (prev >= 0)
        report_taken(nombre, SYM_FUNC, symtab_at(st, prev));

    /* La funcion se registra antes de revisar su cuerpo (sus parametros y locales
       cuelgan de ella). Puede lanzarse a si misma: la recursion es valida porque
       ahora existe el 'shi' para detenerla. */
    int fi = declare(st, nombre->text, SYM_FUNC, rt, SCOPE_GLOBAL,
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
            declare(st, pn->text, SYM_PARAM, t, fi, pn->line, pn->col, 1);
    }

    Symbol *fs = symtab_at(st, fi);
    fs->nparams = params->nchildren;
    fs->param_types = types;
    fs->callable = prev < 0;

    int returns = check_stmts(cuerpo, st, fi);

    if (rt != TY_NADA && !returns) {
        if (has_return(cuerpo))
            SEM_ERROR(nombre, "el hechizo '%s' debe devolver un valor '%s' en todos los caminos "
                              "(falta 'regreso' en algún camino del 'shi')",
                      nombre->text, ctype_name(rt));
        else
            SEM_ERROR(nombre, "el hechizo '%s' debe devolver un valor '%s' (falta 'regreso')",
                      nombre->text, ctype_name(rt));
    }

    if (prev >= 0)
        symtab_truncate(st, fi);    /* funcion repetida: se revisó para dar sus errores, pero no se registra */
}

int semantic_analyze(Node *root, SymTab *st)
{
    nerrors = 0;
    nactive = 0;
    next_block = 0;
    push_block(0);      /* nivel principal del programa */
    check_stmts(root, st, SCOPE_GLOBAL);
    pop_block();
    free(active);
    free(active_line);
    active = active_line = NULL;
    cap_active = 0;
    return nerrors;
}
