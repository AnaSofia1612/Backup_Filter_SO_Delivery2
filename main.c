#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys_smart_copy.h"


//funciones externas del QA performance
extern void copiar_stdio(const char *src, const char *dest); 
extern double medir_tiempo(void (*metodo)(const char*, const char*), const char *src, const char *dest);

int main(int argc, char *argv[]) {
    // Verificación mínima de argumentos
    if (argc < 3) {
        printf("Uso del Sistema de Backup Inteligente:\n");
        printf(" 1. Modo Test (QA):    --test <archivo_origen>\n");
        printf(" 2. Modo Demonio: --daemon <origen1> <destino1> [origen2] [destino2] ... <segundos>\n");
        return 1;
    }

    // --- Validación de Rendimiento ---
    //
    if (strcmp(argv[1], "--test") == 0) {
        printf("Iniciando pruebas de rendimiento sobre: %s\n", argv[2]);
        
        // Mide el motor de syscalls
        double t_sys = medir_tiempo(sys_smart_copy, argv[2], "temp_sys.bak");
        // Mide la función de librería estándar 
        double t_std = medir_tiempo(copiar_stdio, argv[2], "temp_std.bak");
        
        printf("\nResultados Comparativos:\n");
        printf(" > Método Syscalls (Tu motor): %f segundos\n", t_sys);
        printf(" > Método Stdio (Librería C):  %f segundos\n", t_std);
        printf("\nVerificación: Los archivos temporales han sido creados exitosamente.\n");
    } 
    
    //  Motor Multi-archivo en Segundo Plano 
    //
    else if (strcmp(argv[1], "--daemon") == 0) {
        // El último argumento siempre es el tiempo en segundos
        int segundos = atoi(argv[argc - 1]);
        
        // Calculamos cuántas parejas de (origen destino) envió el usuario
        // argc - 2 (quitando programa y --daemon) - 1 (quitando el tiempo)
        int num_archivos = (argc - 3) / 2;

        if (num_archivos < 1) {
            printf("Error: Debes indicar al menos un par de (origen destino) y el tiempo.\n");
            return 1;
        }

        // Creamos los arreglos para las rutas
        char *origenes[num_archivos];
        char *destinos[num_archivos];

        for (int i = 0; i < num_archivos; i++) {
            origenes[i] = argv[2 + (i * 2)];
            destinos[i] = argv[3 + (i * 2)];
        }

        printf("Iniciando servicio de respaldo para %d archivo(s)...\n", num_archivos);
        printf("Intervalo: cada %d segundos.\n", segundos);
        printf("El proceso ahora se ejecutará en segundo plano (Demonio).\n");

        // Llamada a la función de proceso de segundo plano
        //
        iniciar_proceso_segundo_plano(num_archivos, origenes, destinos, segundos);
    } 
    
    else {
        printf("Opción no reconocida. Usa --test o --daemon.\n");
    }

    return 0;
}
