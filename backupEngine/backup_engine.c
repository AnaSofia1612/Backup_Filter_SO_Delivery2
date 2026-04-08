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
    char method[16];   /* "syscall" o "fread" */
} config;


int file_modified(const char *src, const char *dest) {
    struct stat src_stat, dest_stat;

    if (stat(src, &src_stat) != 0) return 0;  /* origen no existe: nada que copiar */
    if (stat(dest, &dest_stat) != 0) return 1; /* destino no existe: hay que copiar */

    /* Copiar si el origen es más nuevo O tiene distinto tamaño */
    return (src_stat.st_mtime > dest_stat.st_mtime) ||
           (src_stat.st_size  != dest_stat.st_size);
}

/* ── Copia usando syscalls directas del kernel ───────────────────── */
int sys_smart_copy(const char *src, const char *dest) {
    if (!file_modified(src, dest)) return 0;

    struct stat st;
    if (stat(src, &st) == -1) {
        perror("[ERROR] El archivo origen no existe");
        return -1;
    }

    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) { perror("[ERROR] open src"); return -1; }

    int fd_dest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (fd_dest < 0) { perror("[ERROR] open dest"); close(fd_src); return -1; }

    char buffer[BUFFER_SIZE];
    ssize_t bytes;

    /* Cada read/write es una syscall — cruce usuario-kernel por llamada */
    while ((bytes = read(fd_src, buffer, BUFFER_SIZE)) > 0) {
        if (write(fd_dest, buffer, bytes) != bytes) {
            perror("[ERROR] write incompleto");
            break;
        }
    }

    close(fd_src);
    close(fd_dest);
    return 1;
}

/* ── Copia usando fread/fwrite (buffering en espacio de usuario) ──── *
 *                                                                      *
 * fread acumula datos en un buffer interno de la libc (por defecto    *
 * 8KB). Para archivos pequeños que caben en ese buffer, todo se lee   *
 * en una sola syscall real, reduciendo los cambios de contexto        *
 * usuario-kernel. Con read() puro, cada llamada implica un cruce al   *
 * kernel aunque el archivo tenga solo unos pocos bytes.               *
 * ─────────────────────────────────────────────────────────────────── */
int fread_copy(const char *src, const char *dest) {
    if (!file_modified(src, dest)) return 0;

    FILE *fsrc = fopen(src, "rb");
    if (!fsrc) { perror("[ERROR] fopen src"); return -1; }

    FILE *fdest = fopen(dest, "wb");
    if (!fdest) { perror("[ERROR] fopen dest"); fclose(fsrc); return -1; }

    char buffer[BUFFER_SIZE];
    size_t bytes;

    /* fread/fwrite usan buffer interno — menos syscalls para archivos pequeños */
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fsrc)) > 0) {
        if (fwrite(buffer, 1, bytes, fdest) != bytes) {
            perror("[ERROR] fwrite incompleto");
            break;
        }
    }

    fclose(fsrc);
    fclose(fdest);
    return 1;
}

void run_backup_cycle(int *copiados, int *saltados) {
    *copiados = 0;
    *saltados = 0;

    /* Seleccionar método según config.yaml */
    int usar_fread = (strcmp(config.method, "fread") == 0);

    for (int i = 0; i < config.file_count; i++) {
        int resultado;

        if (usar_fread)
            resultado = fread_copy(config.source[i], config.destination[i]);
        else
            resultado = sys_smart_copy(config.source[i], config.destination[i]);

        if (resultado == 1)
            (*copiados)++;
        else
            (*saltados)++;
    }
}