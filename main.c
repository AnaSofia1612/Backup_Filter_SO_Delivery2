#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys_smart_copy.h"

// Funciones externas proporcionadas por el Rol 3
extern void copiar_stdio(const char *src, const char *dest); 
extern double medir_tiempo(void (*metodo)(const char*, const char*), const char *src, const char *dest);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso:\n --test <archivo> (Prueba de rendimiento)\n --daemon <src_file> <dest_file> <seg> (Modo Servicio)\n");
        return 1;
    }

    // PARTE DE QA (Rúbrica: Comparativa)
    if (strcmp(argv[1], "--test") == 0) {
        printf("Iniciando pruebas del Rol 3 sobre el archivo: %s\n", argv[2]);
        double t_sys = medir_tiempo(sys_smart_copy, argv[2], "backup_sys.tmp");
        double t_std = medir_tiempo(copiar_stdio, argv[2], "backup_std.tmp");
        
        printf("Resultados:\n - Syscalls: %f s\n - Stdio:    %f s\n", t_sys, t_std);
    } 
    
    // TU PARTE (Modo Demonio para archivos)
    else if (strcmp(argv[1], "--daemon") == 0) {
        if (argc < 5) {
            printf("Error: Faltan argumentos para el modo demonio.\n");
            return 1;
        }
        iniciar_demonio_archivo(argv[2], argv[3], atoi(argv[4]));
    }

    return 0;
}