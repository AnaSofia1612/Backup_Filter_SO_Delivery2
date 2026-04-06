#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFFER_SIZE 4096

/**
 * @brief Copia un archivo usando syscalls.
 * Maneja errores con perror y errno sin cerrar el programa.
 * @param src  Ruta del archivo origen
 * @param dest Ruta del archivo destino
 */
void copiar_syscall(const char *src, const char *dest) {
    char buffer[BUFFER_SIZE];
    struct stat st;

    if (stat(src, &st) == -1) {
        perror("[ERROR] archivo origen no encontrado");
        return;
    }
    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        perror("[ERROR] no se pudo abrir origen");
        return;
    }
    int fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dest < 0) {
        perror("[ERROR] no se pudo abrir destino");
        close(fd_src);
        return;
    }
    ssize_t bytes;
    while ((bytes = read(fd_src, buffer, BUFFER_SIZE)) > 0)
        write(fd_dest, buffer, bytes);

    close(fd_src);
    close(fd_dest);
    printf("[OK] Copia exitosa: %s -> %s\n", src, dest);
}

int main() {
    printf("\n=============================================\n");
    printf("   PRUEBAS DE ROBUSTEZ — Rol 3 QA\n");
    printf("=============================================\n\n");

    // TEST 1: archivo que no existe
    printf("--- TEST 1: Archivo origen inexistente ---\n");
    copiar_syscall("no_existe.txt", "destino.txt");

    // TEST 2: destino en ruta inválida
    printf("\n--- TEST 2: Ruta destino inválida ---\n");
    FILE *f = fopen("prueba_temp.txt", "w");
    fprintf(f, "contenido de prueba");
    fclose(f);
    copiar_syscall("prueba_temp.txt", "/ruta/falsa/archivo.txt");
    remove("prueba_temp.txt");

    // TEST 3: archivo sin permisos de lectura
    printf("\n--- TEST 3: Archivo sin permisos de lectura ---\n");
    FILE *f2 = fopen("sin_permiso.txt", "w");
    fprintf(f2, "secreto");
    fclose(f2);
    chmod("sin_permiso.txt", 0000);
    copiar_syscall("sin_permiso.txt", "destino.txt");
    chmod("sin_permiso.txt", 0644);
    remove("sin_permiso.txt");

    printf("\n=============================================\n");
    printf("Todos los errores fueron manejados sin crash.\n\n");
    return 0;
}