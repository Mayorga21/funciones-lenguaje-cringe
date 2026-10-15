#ifndef TABLE_H
#define TABLE_H

#include <stdio.h>

/* Tabla de texto generica. Sirve para la AST Table y para la Tabla de Simbolos:
   se imprime alineada en consola y se exporta a CSV. */
typedef struct {
    int ncols;
    char **headers;
    char ***rows;
    int nrows;
    int cap;
} Table;

/* Crea una tabla con ncols columnas; los encabezados se pasan como const char* */
Table *table_new(int ncols, ...);

/* Agrega una fila; se deben pasar exactamente ncols cadenas (se copian) */
void table_add_row(Table *t, ...);

void table_print(FILE *out, const Table *t);

/* Devuelve 0 si pudo escribir el archivo, -1 si fallo */
int table_write_csv(const Table *t, const char *path);

void table_free(Table *t);

#endif
