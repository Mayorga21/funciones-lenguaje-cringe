/* Compilador/interprete de Cringe: lexico, sintactico, AST, semantico, tabla de simbolos y ejecucion */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "ast.h"
#include "symtab.h"
#include "semantic.h"
#include "interp.h"
#include "errors.h"
#include "table.h"

extern FILE *yyin;
extern Node *g_root;
int yyparse(void);

static void uso(FILE *out)
{
    fprintf(out,
            "Cringe, el lenguaje del gremio de aventureros\n"
            "Uso: cringe <archivo.cringe> [-o directorio] [--sin-csv]\n"
            "  -o DIR       directorio donde se guardan los CSV (por defecto: salida)\n"
            "  --sin-csv    no exporta los CSV\n"
            "  -h           muestra esta ayuda\n"
            "Códigos de salida: 0 correcto, 1 error léxico, 2 error sintáctico,\n"
            "                   3 error semántico, 4 error de uso o de archivo,\n"
            "                   5 error durante la ejecución\n");
}

static char *read_all(FILE *f)
{
    size_t cap = 4096, n = 0;
    char *buf = malloc(cap);
    if (!buf)
        return NULL;
    size_t r;
    while ((r = fread(buf + n, 1, cap - n - 1, f)) > 0) {
        n += r;
        if (n + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                return NULL;
            }
            buf = tmp;
        }
    }
    buf[n] = '\0';
    return buf;
}

static void export_csv(const char *dir, const char *name, const Table *t)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (table_write_csv(t, path) != 0)
        fprintf(stderr, "aviso: no se pudo escribir %s: %s\n", path, strerror(errno));
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    const char *outdir = "salida";
    int csv = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--ayuda") == 0) {
            uso(stdout);
            return COD_OK;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outdir = argv[++i];
        } else if (strcmp(argv[i], "--sin-csv") == 0) {
            csv = 0;
        } else if (argv[i][0] == '-' || path) {
            uso(stderr);
            return COD_USO;
        } else {
            path = argv[i];
        }
    }
    if (!path) {
        uso(stderr);
        return COD_USO;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "No se pudo abrir '%s': %s\n", path, strerror(errno));
        return COD_USO;
    }
    char *source = read_all(f);
    if (!source) {
        fprintf(stderr, "Sin memoria al leer '%s'\n", path);
        return COD_USO;
    }
    rewind(f);
    yyin = f;
    errors_init(path, source);

    /* Fase 1 y 2: analisis lexico y sintactico (el lexer es llamado por el parser).
       Un error lexico termina el programa desde el lexer; uno sintactico hace fallar yyparse. */
    int parse_rc = yyparse();
    fclose(f);
    if (parse_rc != 0 || !g_root) {
        fprintf(stderr, "Análisis sintáctico: el hechizo falló, game over. No se continúa.\n");
        free(source);
        return COD_SINTACTICO;
    }
    printf("Análisis léxico y sintáctico: correcto (el gremio aprobó tu pergamino)\n");

    ast_assign_ids(g_root);

    /* Fase 3: analisis semantico (reporta todos sus errores) */
    SymTab st;
    symtab_init(&st);
    int nerr = semantic_analyze(g_root, &st);
    if (nerr == 0)
        printf("Análisis semántico: correcto (mazmorra sin bugs)\n");
    else
        printf("Análisis semántico: %d error(es) (la mazmorra tiene bugs)\n", nerr);

    printf("\nÁrbol de sintaxis abstracta (AST): el árbol de habilidades\n");
    ast_print_tree(stdout, g_root);

    Table *ast_t = ast_to_table(g_root);
    printf("\nAST Table\n");
    table_print(stdout, ast_t);

    /* Fase 4: ejecucion (solo si no hubo errores) */
    if (nerr == 0) {
        printf("\nEjecución: comienza la aventura\n");
        char *salida = interp_run(g_root, &st, stdout);
        if (salida[0])
            printf("\nSalida del programa (lo que narra el bardo)\n%s", salida);
        free(salida);
    } else {
        printf("\nNo se ejecuta el programa porque hay errores semánticos: el jefe final no puede ser invocado.\n");
    }

    Table *sym_t = symtab_to_table(&st, nerr == 0);
    printf("\nTabla de símbolos: el bestiario\n");
    table_print(stdout, sym_t);

    if (csv) {
        if (mkdir(outdir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "aviso: no se pudo crear el directorio '%s': %s\n",
                    outdir, strerror(errno));
        } else {
            export_csv(outdir, "ast_table.csv", ast_t);
            export_csv(outdir, "tabla_simbolos.csv", sym_t);
            printf("\nCSV exportados en: %s/\n", outdir);
        }
    }

    table_free(ast_t);
    table_free(sym_t);
    symtab_free(&st);
    ast_free(g_root);
    free(source);

    return nerr ? COD_SEMANTICO : COD_OK;
}
