#ifndef ERRORS_H
#define ERRORS_H

/* Codigos de salida del compilador */
enum {
    COD_OK = 0,
    COD_LEXICO = 1,
    COD_SINTACTICO = 2,
    COD_SEMANTICO = 3,
    COD_USO = 4,
    COD_EJECUCION = 5
};

/* Nombre del archivo fuente y su contenido completo (para mostrar la linea con error) */
extern const char *g_filename;
extern const char *g_source;

void errors_init(const char *filename, const char *source);

/* Imprime un error en stderr con el formato archivo:linea:columna: tipo: mensaje
   y muestra la linea del codigo fuente con un indicador de columna. */
void report_error(const char *kind, int line, int col, const char *fmt, ...);

#endif
