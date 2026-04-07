#include "sys_smart_copy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

/* Estructura definida en config_parser.c — importada sin redefinir */
extern struct config_t {
    char source[100][256];
    char destination[100][256];
    int  file_count;
    int  interval;
} config;


int file_modified(const char *src, const char *dest) {
    struct stat src_stat, dest_stat;

    if (stat(src, &src_stat) != 0) return 0;  /* origen no existe: nada que copiar */
    if (stat(dest, &dest_stat) != 0) return 1; /* destino no existe: hay que copiar */

    /* Copiar si el origen es más nuevo O tiene distinto tamaño */
    return (src_stat.st_mtime > dest_stat.st_mtime) ||
           (src_stat.st_size  != dest_stat.st_size);
}

void run_backup_cycle(int *copiados, int *saltados) {
    *copiados = 0;
    *saltados = 0;

    for (int i = 0; i < config.file_count; i++) {
        if (!file_modified(config.source[i], config.destination[i])) {
            (*saltados)++;
            continue;
        }

        int fd_src = open(config.source[i], O_RDONLY);
        int fd_dest = open(config.destination[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd_src < 0 || fd_dest < 0) {
            perror("File error");
            (*saltados)++;
            continue;
        }

        char buffer[4096];
        ssize_t bytes;

        while ((bytes = read(fd_src, buffer, sizeof(buffer))) > 0) {
            write(fd_dest, buffer, bytes);
        }

        close(fd_src);
        close(fd_dest);
        (*copiados)++;
    }
}


//  Copia por Syscalls 

int sys_smart_copy(const char *src, const char *dest) {
    if (!file_modified(src, dest)) {
        return 0;
    }

    int fd_src, fd_dest;
    ssize_t bytes;
    char buffer[BUFFER_SIZE];
    struct stat st;

    // Validación de existencia con la funcion stat que devulve los metadatos del archivo
    if (stat(src, &st) == -1) {
        perror("[ERROR] El archivo origen no existe");
        return -1;
    }

    fd_src = open(src, O_RDONLY); // funciones que se realzan directamente con el kernel (open, write, etc)
    if (fd_src < 0) return -1;

    // Se crea el destino con los mismos permisos del original
    fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dest < 0) { 
        close(fd_src); 
        return -1; 
    }

    // Transferencia usando el buffer de página (4KB)
    while ((bytes = read(fd_src, buffer, BUFFER_SIZE)) > 0) {
        if (write(fd_dest, buffer, bytes) != bytes) break;
    }

    close(fd_src);
    close(fd_dest);
    return 0;
}


void iniciar_proceso_segundo_plano(int cantidad, char *archivos_src[], char *archivos_dest[], int segundos) {
    pid_t pid = fork(); // Creación del proceso hijo, crea una copia del programa en ese instante
    if (pid > 0) exit(0); // El padre termina para liberar la terminal, esto produce que el hijo quede en segundo plano
    if (pid < 0) exit(1);

    setsid(); // Independencia del proceso, es decir independiente de la terminal si la cerramos sigue presente
    while (1) {
        
        // Ciclo que recorre la cantidad de archivos decidida por el usuario
        for (int i = 0; i < cantidad; i++) {
            sys_smart_copy(archivos_src[i], archivos_dest[i]);
        }
        sleep(segundos); // Intervalo de espera, el usuario decide el tiempo en cual el programa es despierta y respalda el archivo
    }
}