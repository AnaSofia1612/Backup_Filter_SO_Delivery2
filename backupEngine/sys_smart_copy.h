#ifndef SYS_SMART_COPY_H
#define SYS_SMART_COPY_H
// Tamaño de página estándar (4KB). 
// Optimiza el movimiento de datos al alinearse con los bloques del Kernel.
#define BUFFER_SIZE 4096

// Copia usando syscalls directas del kernel (open, read, write, close)
int sys_smart_copy(const char *src, const char *dest);

// Copia usando fread/fwrite — buffering en espacio de usuario
// Más eficiente en archivos pequeños por reducir cambios de contexto usuario-kernel
int fread_copy(const char *src, const char *dest);

// Función de monitoreo: Lógica del demonio para archivos, 
// es decir es aquella que implementamos para que el proceso se realice en segundo plano
void iniciar_proceso_segundo_plano(int cantidad, char *archivos_src[], char *archivos_dest[], int segundos);

#endif