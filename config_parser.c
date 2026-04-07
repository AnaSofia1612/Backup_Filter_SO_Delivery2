/*
 * config_parser.c — Parser YAML + panel de estado (PLUS #2)
 * ─────────────────────────────────────────────────────────────────
 * Define la estructura global `config` que comparten todos los módulos.
 *
 * Formato del config.yaml:
 *   interval: 10
 *   files:
 *     - source: /ruta/origen.txt
 *       destination: /ruta/destino.txt
 *
 * PLUS #1 — SIGHUP: Recarga en caliente
 *   El daemon captura SIGHUP y llama a reload_config().
 *   Sin detener el proceso, aplica el nuevo intervalo del YAML.
 *   Uso: kill -HUP $(cat /tmp/backup_daemon.pid)
 *
 * PLUS #2 — Panel de estado /proc-style
 *   Después de cada ciclo, status_write() actualiza /tmp/backup_status
 *   con métricas en tiempo real usando rename() atómico.
 *   Uso: cat /tmp/backup_status
 * ─────────────────────────────────────────────────────────────────
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <yaml.h>

#define MAX_PATH  256
#define MAX_FILES 100
#define PID_FILE    "/tmp/backup_daemon.pid"
#define STATUS_FILE "/tmp/backup_status"

/* ══════════════════════════════════════════════════════════════════
 * Estructura de configuración — definida UNA SOLA VEZ aquí.
 * Todos los módulos la importan con: extern struct config_t config;
 * ══════════════════════════════════════════════════════════════════ */
struct config_t {
    char source[MAX_FILES][MAX_PATH];
    char destination[MAX_FILES][MAX_PATH];
    int  file_count;
    int  interval;
} config;

/* Ruta del YAML activo (necesaria para recarga SIGHUP) */
static char g_yaml_path[MAX_PATH] = "";

/* Flags de señales — volatile sig_atomic_t: seguro desde signal handlers */
volatile sig_atomic_t g_reload  = 0;  /* SIGHUP → releer YAML  */
volatile sig_atomic_t g_running = 1;  /* SIGTERM → salir limpio */

/* ══════════════════════════════════════════════════════════════════
 * parse_yaml_internal() — lógica de parseo reutilizable
 * ══════════════════════════════════════════════════════════════════ */
static int parse_yaml_internal(const char *filename, struct config_t *cfg) {
    FILE *fh = fopen(filename, "r");
    if (!fh) {
        perror("[ERROR] No se pudo abrir el archivo de configuración");
        return -1;
    }

    yaml_parser_t parser;
    yaml_event_t  event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    memset(cfg, 0, sizeof(struct config_t));
    cfg->interval = 3600;  /* valor por defecto */

    char last_key[64] = "";
    int  is_key       = 1;
    int  in_files     = 0;
    int  done         = 0;

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "[ERROR] YAML parse error en línea %zu\n",
                    parser.problem_mark.line + 1);
            break;
        }

        switch (event.type) {

        case YAML_MAPPING_START_EVENT:
            is_key = 1;
            break;

        case YAML_MAPPING_END_EVENT:
            is_key = 1;
            break;

        case YAML_SEQUENCE_START_EVENT:
            if (strcmp(last_key, "files") == 0) in_files = 1;
            break;

        case YAML_SEQUENCE_END_EVENT:
            in_files = 0;
            break;

        case YAML_SCALAR_EVENT: {
            const char *val = (const char *)event.data.scalar.value;

            if (is_key) {
                strncpy(last_key, val, sizeof(last_key) - 1);
                is_key = 0;
            } else {
                if (strcmp(last_key, "interval") == 0) {
                    cfg->interval = atoi(val);

                } else if (strcmp(last_key, "source") == 0 && in_files) {
                    if (cfg->file_count < MAX_FILES)
                        strncpy(cfg->source[cfg->file_count],
                                val, MAX_PATH - 1);

                } else if (strcmp(last_key, "destination") == 0 && in_files) {
                    strncpy(cfg->destination[cfg->file_count],
                            val, MAX_PATH - 1);
                    cfg->file_count++;
                }
                is_key = 1;
            }
            break;
        }

        case YAML_STREAM_END_EVENT:
            done = 1;
            break;

        default:
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return (cfg->file_count > 0) ? 0 : -1;
}

/* ══════════════════════════════════════════════════════════════════
 * parse_yaml() — carga inicial, llamada desde main()
 * ══════════════════════════════════════════════════════════════════ */
void parse_yaml(const char *filename) {
    strncpy(g_yaml_path, filename, MAX_PATH - 1);

    if (parse_yaml_internal(filename, &config) < 0) {
        fprintf(stderr, "[ERROR] El YAML no tiene pares source/destination\n");
        exit(EXIT_FAILURE);
    }

    printf("[CONFIG] Intervalo : %d segundos\n", config.interval);
    printf("[CONFIG] Archivos  : %d par(es)\n",  config.file_count);
    for (int i = 0; i < config.file_count; i++)
        printf("[CONFIG]   [%d] %s  →  %s\n",
               i + 1, config.source[i], config.destination[i]);
}

/* ══════════════════════════════════════════════════════════════════
 * PLUS #1 — reload_config()
 * Llamada desde el bucle del daemon cuando g_reload == 1.
 * Relee el YAML y aplica los nuevos valores SIN detener el proceso.
 * ══════════════════════════════════════════════════════════════════ */
void reload_config(void) {
    struct config_t new_cfg;
    printf("[SIGHUP] Recargando configuración desde %s...\n", g_yaml_path);
    fflush(stdout);

    if (parse_yaml_internal(g_yaml_path, &new_cfg) == 0) {
        config = new_cfg;
        printf("[SIGHUP] OK — nuevo intervalo: %ds | archivos: %d\n",
               config.interval, config.file_count);
    } else {
        printf("[SIGHUP] ERROR al recargar — se mantiene la config anterior\n");
    }
    fflush(stdout);
}

/* ══════════════════════════════════════════════════════════════════
 * PLUS #2 — status_write()
 * Escribe /tmp/backup_status de forma atómica (rename).
 * Imita cómo el kernel expone info en /proc/[pid]/status.
 *
 * Parámetros:
 *   estado   → "CORRIENDO", "EN_CICLO", "DETENIDO"
 *   ciclos   → total de ciclos ejecutados
 *   copiados → archivos copiados en total
 *   saltados → archivos saltados (sin cambios) en total
 *   proximo  → timestamp del próximo ciclo
 * ══════════════════════════════════════════════════════════════════ */
void status_write(const char *estado, int ciclos,
                  int copiados, int saltados, time_t proximo) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", STATUS_FILE);

    FILE *f = fopen(tmp, "w");
    if (!f) return;

    /* Formatear el próximo ciclo como timestamp legible */
    char s_proximo[32] = "N/A";
    long secs_restantes = 0;
    if (proximo > 0) {
        struct tm *t = localtime(&proximo);
        strftime(s_proximo, sizeof(s_proximo), "%Y-%m-%d %H:%M:%S", t);
        secs_restantes = (long)(proximo - time(NULL));
        if (secs_restantes < 0) secs_restantes = 0;
    }

    fprintf(f,
        "PID=%d\n"
        "STATE=%s\n"
        "INTERVALO_SEG=%d\n"
        "TOTAL_CICLOS=%d\n"
        "TOTAL_COPIADOS=%d\n"
        "TOTAL_SALTADOS=%d\n"
        "PROXIMO_CICLO=%s\n"
        "SEGS_RESTANTES=%ld\n",
        (int)getpid(), estado,
        config.interval,
        ciclos, copiados, saltados,
        s_proximo, secs_restantes
    );
    fclose(f);

    /* rename() es atómico en Linux — el lector nunca ve el archivo a medias */
    rename(tmp, STATUS_FILE);
}

/* ══════════════════════════════════════════════════════════════════
 * Manejadores de señales
 * REGLA: solo asignan flags, nunca hacen I/O ni llaman funciones
 *        que no sean async-signal-safe.
 * ══════════════════════════════════════════════════════════════════ */
static void handle_sighup(int sig)  { (void)sig; g_reload  = 1; }
static void handle_sigterm(int sig) { (void)sig; g_running = 0; }

/* ══════════════════════════════════════════════════════════════════
 * setup_signals() — registrar manejadores, llamar desde el daemon
 * ══════════════════════════════════════════════════════════════════ */
void setup_signals(void) {
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = handle_sighup;
    sa.sa_flags   = 0;   /* sin SA_RESTART → sleep() se interrumpe al llegar SIGHUP */
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    /* Guardar PID para que el usuario pueda enviar señales fácilmente */
    FILE *pf = fopen(PID_FILE, "w");
    if (pf) { fprintf(pf, "%d\n", (int)getpid()); fclose(pf); }
}

/* ══════════════════════════════════════════════════════════════════
 * cleanup() — limpiar al salir limpiamente
 * ══════════════════════════════════════════════════════════════════ */
void cleanup(void) {
    status_write("DETENIDO", 0, 0, 0, 0);
    unlink(PID_FILE);
}