#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFER_SIZE 4096

/**
 * @brief Copia un archivo usando syscalls de bajo nivel.
 * Usa open, read, write y close con buffer de 4096 bytes.
 * @param src  Ruta del archivo origen
 * @param dest Ruta del archivo destino
 */
void copiar_syscall(const char *src, const char *dest) {
    char buffer[BUFFER_SIZE];
    struct stat st;

    if (stat(src, &st) == -1) {
        perror("[ERROR] stat");
        return;
    }
    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        perror("[ERROR] open origen");
        return;
    }
    int fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dest < 0) {
        perror("[ERROR] open destino");
        close(fd_src);
        return;
    }
    ssize_t bytes;
    while ((bytes = read(fd_src, buffer, BUFFER_SIZE)) > 0)
        write(fd_dest, buffer, bytes);

    close(fd_src);
    close(fd_dest);
}

/**
 * @brief Copia un archivo usando la librería estándar de C.
 * Usa fopen, fread, fwrite y fclose.
 * @param src  Ruta del archivo origen
 * @param dest Ruta del archivo destino
 */
void copiar_stdio(const char *src, const char *dest) {
    char buffer[BUFFER_SIZE];

    FILE *f_src = fopen(src, "rb");
    if (!f_src) {
        perror("[ERROR] fopen origen");
        return;
    }
    FILE *f_dest = fopen(dest, "wb");
    if (!f_dest) {
        perror("[ERROR] fopen destino");
        fclose(f_src);
        return;
    }
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, f_src)) > 0)
        fwrite(buffer, 1, bytes, f_dest);

    fclose(f_src);
    fclose(f_dest);
}

/**
 * @brief Crea un archivo de prueba con el tamaño indicado en bytes.
 * @param nombre Ruta del archivo a crear
 * @param bytes  Tamaño en bytes
 */
void crear_archivo_prueba(const char *nombre, long bytes) {
    FILE *f = fopen(nombre, "wb");
    if (!f) {
        perror("[ERROR] crear archivo de prueba");
        return;
    }
    char buf[4096] = {0};
    long restante = bytes;
    while (restante > 0) {
        long chunk = restante < 4096 ? restante : 4096;
        fwrite(buf, 1, chunk, f);
        restante -= chunk;
    }
    fclose(f);
}

/**
 * @brief Mide el tiempo en segundos que tarda una función de copia.
 * @param metodo Puntero a la función de copia
 * @param src    Archivo origen
 * @param dest   Archivo destino
 * @return Tiempo en segundos como double
 */
double medir_tiempo(void (*metodo)(const char*, const char*),
                    const char *src, const char *dest) {
    clock_t inicio = clock();
    metodo(src, dest);
    clock_t fin = clock();
    return (double)(fin - inicio) / CLOCKS_PER_SEC;
}

int main() {
    printf("\n=============================================\n");
    printf("   PRUEBA DE RENDIMIENTO — QA\n");
    printf("=============================================\n\n");

    long tamanos[]    = { 1024, 1024*1024, 1024*1024*100 };
    char *etiquetas[] = { "1 KB", "1 MB", "100 MB" };
    int n = 3;

    printf("%-8s | %-15s | %-15s\n", "Tamaño", "Syscall (seg)", "stdio (seg)");
    printf("---------|-----------------|----------------\n");

    for (int i = 0; i < n; i++) {
        printf("Creando archivo de prueba: %s...\n", etiquetas[i]);
        crear_archivo_prueba("archivo_prueba.tmp", tamanos[i]);

        double t_syscall = medir_tiempo(copiar_syscall,
                                        "archivo_prueba.tmp",
                                        "copia_syscall.tmp");
        double t_stdio   = medir_tiempo(copiar_stdio,
                                        "archivo_prueba.tmp",
                                        "copia_stdio.tmp");

        printf("%-8s | %-15.6f | %-15.6f\n", etiquetas[i], t_syscall, t_stdio);

        remove("archivo_prueba.tmp");
        remove("copia_syscall.tmp");
        remove("copia_stdio.tmp");
    }

    printf("\n=============================================\n");
    printf("Prueba completada.\n\n");
    return 0;
}