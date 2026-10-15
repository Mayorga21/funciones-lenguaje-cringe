#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "table.h"

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

Table *table_new(int ncols, ...)
{
    Table *t = calloc(1, sizeof *t);
    if (!t) {
        perror("calloc");
        exit(1);
    }
    t->ncols = ncols;
    t->headers = malloc(sizeof(char *) * ncols);

    va_list ap;
    va_start(ap, ncols);
    for (int i = 0; i < ncols; i++)
        t->headers[i] = dup(va_arg(ap, const char *));
    va_end(ap);
    return t;
}

void table_add_row(Table *t, ...)
{
    if (t->nrows == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->rows = realloc(t->rows, sizeof(char **) * t->cap);
        if (!t->rows) {
            perror("realloc");
            exit(1);
        }
    }

    char **row = malloc(sizeof(char *) * t->ncols);
    va_list ap;
    va_start(ap, t);
    for (int i = 0; i < t->ncols; i++)
        row[i] = dup(va_arg(ap, const char *));
    va_end(ap);
    t->rows[t->nrows++] = row;
}

/* Ancho visible de una cadena UTF-8 (cuenta caracteres, no bytes) */
static int display_width(const char *s)
{
    int w = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80)
            w++;
    return w;
}

static void print_cell(FILE *out, const char *s, int width, int last)
{
    fputs(s, out);
    if (!last) {
        for (int i = display_width(s); i < width + 2; i++)
            fputc(' ', out);
    }
}

void table_print(FILE *out, const Table *t)
{
    int *w = calloc(t->ncols, sizeof(int));

    for (int c = 0; c < t->ncols; c++) {
        w[c] = display_width(t->headers[c]);
        for (int r = 0; r < t->nrows; r++) {
            int cw = display_width(t->rows[r][c]);
            if (cw > w[c])
                w[c] = cw;
        }
    }

    for (int c = 0; c < t->ncols; c++)
        print_cell(out, t->headers[c], w[c], c == t->ncols - 1);
    fputc('\n', out);

    /* Subrayado simple del encabezado */
    for (int c = 0; c < t->ncols; c++) {
        for (int i = 0; i < w[c]; i++)
            fputc('-', out);
        if (c != t->ncols - 1)
            fputs("  ", out);
    }
    fputc('\n', out);

    for (int r = 0; r < t->nrows; r++) {
        for (int c = 0; c < t->ncols; c++)
            print_cell(out, t->rows[r][c], w[c], c == t->ncols - 1);
        fputc('\n', out);
    }
    free(w);
}

static void csv_field(FILE *f, const char *s)
{
    int quote = strpbrk(s, ",\"\n\r") != NULL;
    if (!quote) {
        fputs(s, f);
        return;
    }
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"')
            fputc('"', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

int table_write_csv(const Table *t, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;

    for (int c = 0; c < t->ncols; c++) {
        csv_field(f, t->headers[c]);
        fputc(c == t->ncols - 1 ? '\n' : ',', f);
    }
    for (int r = 0; r < t->nrows; r++)
        for (int c = 0; c < t->ncols; c++) {
            csv_field(f, t->rows[r][c]);
            fputc(c == t->ncols - 1 ? '\n' : ',', f);
        }

    fclose(f);
    return 0;
}

void table_free(Table *t)
{
    if (!t)
        return;
    for (int c = 0; c < t->ncols; c++)
        free(t->headers[c]);
    free(t->headers);
    for (int r = 0; r < t->nrows; r++) {
        for (int c = 0; c < t->ncols; c++)
            free(t->rows[r][c]);
        free(t->rows[r]);
    }
    free(t->rows);
    free(t);
}
