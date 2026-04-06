#ifndef SYS_SMART_COPY_H
#define SYS_SMART_COPY_H

#define BUFFER_SIZE 4096

// Función núcleo: Copia binaria optimizada (Sección 1)
int sys_smart_copy(const char *src, const char *dest);

// Función de monitoreo: Lógica del demonio para archivos (Sección 4)
void iniciar_demonio_archivo(const char *src, const char *dest, int segundos);

#endif