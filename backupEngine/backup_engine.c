#include "sys_smart_copy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

// SECCIÓN 1: Copia por Syscalls (Requisito de Rúbrica)
// Implementación idéntica a la validada por QA
int sys_smart_copy(const char *src, const char *dest) {
    int fd_src, fd_dest;
    ssize_t bytes;
    char buffer[BUFFER_SIZE];
    struct stat st;

    // Validación de existencia
    if (stat(src, &st) == -1) {
        perror("[ERROR] El archivo origen no existe");
        return -1;
    }

    fd_src = open(src, O_RDONLY);
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

// SECCIÓN 4: El Demonio (Enfocado solo en un archivo)
void iniciar_demonio_archivo(const char *src, const char *dest, int segundos) {
    pid_t pid = fork(); // Creación del proceso hijo
    if (pid > 0) exit(0); // El padre termina para liberar la terminal
    if (pid < 0) exit(1);

    setsid(); // Independencia del proceso
    while (1) {
        // Ejecuta la copia del archivo directamente
        if (sys_smart_copy(src, dest) == 0) {
            // Opcional: Podrías escribir en un log aquí
        }
        sleep(segundos); // Intervalo de espera
    }
}