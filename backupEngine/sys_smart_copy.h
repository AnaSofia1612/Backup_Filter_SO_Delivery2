#ifndef SYS_SMART_COPY_H
#define SYS_SMART_COPY_H
// Tamaño de página estándar (4KB). 
// Optimiza el movimiento de datos al alinearse con los bloques del Kernel.
#define BUFFER_SIZE 4096

// Función: realiza una copia de bit a bit 
int sys_smart_copy(const char *src, const char *dest);

// Función de monitoreo: Lógica del demonio para archivos, 
//es decir es aquella que implementamos para que el proceso se realice en segundo plano
void iniciar_proceso_segundo_plano(int cantidad, char *archivos_src[], char *archivos_dest[], int segundos);

#endif
