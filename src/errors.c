#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "errors.h"

const char *g_filename = "";
const char *g_source = "";

void errors_init(const char *filename, const char *source)
{
    g_filename = filename;
    g_source = source;
}

static void print_context(int line, int col)
{
    const char *p = g_source;
    if (!p || line < 1)
        return;

    for (int l = 1; l < line; l++) {
        p = strchr(p, '\n');
        if (!p)
            return;
        p++;
    }

    const char *end = strchr(p, '\n');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len > 0 && p[len - 1] == '\r')
        len--;

    fprintf(stderr, "    %.*s\n    ", (int)len, p);

    /* Espacios hasta la columna del error (se saltan los bytes de continuacion UTF-8) */
    for (int i = 0; i < col - 1 && i < (int)len; i++) {
        unsigned char c = (unsigned char)p[i];
        if ((c & 0xC0) == 0x80)
            continue;
        fputc(c == '\t' ? '\t' : ' ', stderr);
    }
    fputs("^\n", stderr);
}

void report_error(const char *kind, int line, int col, const char *fmt, ...)
{
    fflush(stdout);
    fprintf(stderr, "%s:%d:%d: %s: ", g_filename, line, col, kind);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
    print_context(line, col);
}
