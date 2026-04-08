
/*
 * main.c — Punto de entrada del sistema de backup inteligente
 * ─────────────────────────────────────────────────────────────────
 * Uso: ./backup_app config.yaml
 * ─────────────────────────────────────────────────────────────────
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include "backupEngine/sys_smart_copy.h"

/* ── Configuración global definida en config_parser.c ───────────── */
struct config_t {
    char source[100][256];
    char destination[100][256];
    int  file_count;
    int  interval;
};
extern struct config_t config;

/* ── Funciones exportadas por config_parser.c ───────────────────── */
extern void parse_yaml(const char *filename);
extern void reload_config(void);
extern void setup_signals(void);
extern void cleanup(void);
extern void status_write(const char *estado, int ciclos,
                         int copiados, int saltados, time_t proximo);

/* ── Flags de señales definidos en config_parser.c ─────────────── */
extern volatile sig_atomic_t g_reload;
extern volatile sig_atomic_t g_running;

/* ── Función del motor de copia (backup_engine.c) ───────────────── */
extern void run_backup_cycle(int *copiados, int *saltados);

/* ──────────────────────────────────────────────────────────────────
 * Versión instrumentada de run_backup_cycle que cuenta resultados
 * para el panel de estado (PLUS #2).
 *
 * Nota: run_backup_cycle() ya imprime [COPIA]/[SKIP]/[OK] en el log.
 * Aquí solo contamos cuántos archivos se procesaron revisando si el
 * destino fue modificado (mtime más reciente que antes del ciclo).
 * ────────────────────────────────────────────────────────────────── */
static void run_cycle_with_stats(int *copiados_total, int *saltados_total) {
    int copiados = 0, saltados = 0;
    run_backup_cycle(&copiados, &saltados);
    *copiados_total += copiados;
    *saltados_total += saltados;
}

/* ──────────────────────────────────────────────────────────────────
 * start_daemon()
 *
 * Flujo completo:
 *   1. fork()    → divide el proceso en padre e hijo
 *   2. Padre     → imprime PID y sale (terminal queda libre)
 *   3. Hijo      → setsid() desconecta del terminal
 *   4. Hijo      → chdir() al directorio original (rutas relativas OK)
 *   5. Hijo      → redirige stdout/stderr al log
 *   6. Hijo      → setup_signals() registra SIGHUP y SIGTERM
 *   7. Hijo      → while(1): backup → status → sleep → check señales
 * ────────────────────────────────────────────────────────────────── */
void start_daemon(const char *working_dir) {
    pid_t pid = fork();

    if (pid < 0) { perror("fork failed"); exit(EXIT_FAILURE); }

    /* ── PADRE: informa y sale ──────────────────────────────────── */
    if (pid > 0) {
        printf("\n");
        printf("  [DAEMON] PID              : %d\n", pid);
        printf("  [DAEMON] Log              : tail -f /tmp/backup.log\n");
        printf("  [DAEMON] Estado           : cat /tmp/backup_status\n");
        printf("  [DAEMON] Detener          : kill %d\n", pid);
        printf("  [DAEMON] Recargar config_parser (Si agregaste nuevos archivos): kill -HUP %d\n\n", pid);
        exit(EXIT_SUCCESS);
    }

    /* ── HIJO: preparar daemon ──────────────────────────────────── */
    if (setsid() < 0)            { perror("setsid");  exit(EXIT_FAILURE); }
    if (chdir(working_dir) < 0)  { perror("chdir");   exit(EXIT_FAILURE); }
    umask(0);

    freopen("/tmp/backup.log", "a", stdout);
    freopen("/tmp/backup.log", "a", stderr);

    /* Registrar señales y guardar PID file */
    setup_signals();

    int    total_ciclos   = 0;
    int    total_copiados = 0;
    int    total_saltados = 0;

    printf("[DAEMON] Arrancando | PID=%d | Intervalo=%ds | Archivos=%d\n",
           (int)getpid(), config.interval, config.file_count);
    fflush(stdout);

    /* ── Bucle principal del daemon ─────────────────────────────── */
    while (g_running) {

        /* ── PLUS #1: Comprobar si llegó SIGHUP ─────────────────── */
        if (g_reload) {
            g_reload = 0;
            reload_config();
        }

        /* ── Ejecutar ciclo y actualizar panel ──────────────────── */
        status_write("EN_CICLO", total_ciclos,
                     total_copiados, total_saltados, 0);

        printf("[CICLO #%d] Iniciando...\n", total_ciclos + 1);
        fflush(stdout);

        /* Snapshot ANTES del ciclo para calcular el delta de este ciclo */
        int copiados_antes = total_copiados;
        int saltados_antes = total_saltados;

        run_cycle_with_stats(&total_copiados, &total_saltados);
        total_ciclos++;

        /* ── PLUS #2: Panel de estado actualizado ───────────────── */
        time_t proximo = time(NULL) + config.interval;
        status_write("CORRIENDO", total_ciclos,
                     total_copiados, total_saltados, proximo);

        printf("[CICLO #%d] Completado — copiados: %d | saltados: %d\n",
               total_ciclos,
               total_copiados - copiados_antes,
               total_saltados - saltados_antes);
        printf("[DAEMON] Durmiendo %ds...\n", config.interval);
        fflush(stdout);

        /* sleep() fragmentado para reaccionar a SIGHUP rápido */
        int restante = config.interval;
        while (restante > 0 && g_running && !g_reload)
            restante = (int)sleep(restante);
    }

    /* ── Salida limpia ──────────────────────────────────────────── */
    printf("[DAEMON] Detenido limpiamente.\n");
    fflush(stdout);
    cleanup();
    exit(EXIT_SUCCESS);
}

/* ──────────────────────────────────────────────────────────────────
 * main()
 * ────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <config.yaml>\n\n", argv[0]);
        printf("  Ejemplo: ./backup_app config.yaml\n");
        printf("  Ver log : tail -f /tmp/backup.log\n");
        printf("  Estado  : cat /tmp/backup_status\n");
        return 1;
    }

    /* Guardar directorio de trabajo ANTES de fork/chdir */
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) { perror("getcwd"); return 1; }

    /* Cargar configuración */
    parse_yaml(argv[1]);

    if (config.file_count == 0) {
        fprintf(stderr,
            "[ERROR] El YAML no tiene pares source/destination.\n"
            "        Ejemplo de formato correcto:\n\n"
            "  interval: 10\n"
            "  files:\n"
            "    - source: archivo.txt\n"
            "      destination: copia.txt\n\n");
        return 1;
    }

    start_daemon(cwd);
    return 0;
}

