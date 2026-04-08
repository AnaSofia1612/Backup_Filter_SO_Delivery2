# Backup_Filter_SO_Delivery2

Sistema de backup inteligente con integración a la shell **EAFITos**.  
Copia archivos de forma eficiente usando syscalls, corre como daemon en segundo plano y se puede invocar directamente desde la shell del proyecto de Sistema Operativo.

---

## Requisitos previos

- Sistema Linux o WSL (Windows Subsystem for Linux)
- GCC con soporte C11
- Librería `libyaml`

Instalar `libyaml` si no la tienes:

```bash
sudo apt-get install libyaml-dev
```

---

## Estructura del proyecto:
Backup_Filter_SO_Delivery2/
├── main.c                  # Punto de entrada del sistema
├── config_parser.c         # Parseo del archivo YAML de configuración
├── config.yaml             # Archivo de configuración de ejemplo
├── backupEngine/
│   ├── backup_engine.c     # Motor de copia con syscalls
│   └── sys_smart_copy.h    # Header del motor
├── QA_Performance/
│   ├── performance_test.c  # Pruebas de rendimiento
│   └── test_errores.c      # Pruebas de manejo de errores
├── Makefile
└── README.md

---

## Compilación

Desde la carpeta raíz del proyecto:

```bash
make
```

Si todo va bien verás:
✓  Compilado correctamente → ./backup_app

Para limpiar los binarios:

```bash
make clean
```

---

## Uso directo (sin la shell)
El sistema usa un archivo `config.yaml` para definir qué archivos respaldar y cada cuánto tiempo.

### Formato del config.yaml

```yaml
interval: 10
files:
  - source: prueba.txt
    destination: copia.txt
  - source: archivo2.txt
    destination: backup_archivo2.txt
```

### Correr el daemon

```bash
./backup_app config.yaml
```

El proceso se ejecuta en **segundo plano automáticamente** e imprime el PID:
[DAEMON] PID    : 2369
[DAEMON] Log    : tail -f /tmp/backup.log
[DAEMON] Estado : cat /tmp/backup_status
[DAEMON] Detener: kill 2369

### Ver el log en tiempo real

```bash
tail -f /tmp/backup.log
```

### Ver el estado del daemon

```bash
cat /tmp/backup_status
```

### Detener el daemon

```bash
make stop
```

### Recargar configuración sin detener el daemon

```bash
make reload
```

---

## Integración con la shell EAFITos

Este proyecto está integrado con la shell **EAFITos** (repositorio SISTEMAOPERATIVO).  
El comando `respaldo` permite invocar el sistema de backup directamente desde la shell.

### Estructura de carpetas requerida

Las dos carpetas deben estar **al mismo nivel**:
SISTEMAS OPERATIVOS/
├── Backup_Filter_SO_Delivery2/   ← este proyecto
└── SistemaOperativo/             ← la shell EAFITos

### Pasos para correr la integración

**1. Compilar el backup (este proyecto):**

```bash
cd Backup_Filter_SO_Delivery2
sudo apt-get install libyaml-dev
make
```

**2. Compilar la shell EAFITos:**

```bash
cd ../SistemaOperativo
make
```

**3. Correr la shell:**

```bash
./build/sistema_os
```

**4. Usar el comando `respaldo` desde la shell:** 
EAFITos> respaldo --iniciar <archivo_origen> <archivo_destino>

Ejemplo:
EAFITos> respaldo --iniciar prueba.txt copia.txt

Salida esperada:
Iniciando respaldo de 'prueba.txt' en 'copia.txt'...
[CONFIG] Intervalo : 10 segundos
[CONFIG] Archivos  : 1 par(es)
[CONFIG]   [1] prueba.txt  →  copia.txt
[DAEMON] PID    : 2369
[DAEMON] Log    : tail -f /tmp/backup.log
[DAEMON] Detener: kill 2369

**5. Verificar que el backup está corriendo:**

```bash
tail -f /tmp/backup.log
```

**6. Detener el daemon cuando termine:**

```bash
kill <PID>
```

---

## Cómo funciona internamente

1. La shell EAFITos recibe el comando `respaldo --iniciar origen destino`
2. `backup_commands.c` genera un archivo `config.yaml` temporal en `/tmp/respaldo_temp.yaml`
3. Llama al ejecutable `backup_app` con ese YAML
4. `backup_app` hace `fork()` y lanza un daemon en segundo plano
5. El daemon copia archivos cada 10 segundos usando syscalls (`open`, `read`, `write`)
6. Solo copia si el archivo cambió (backup inteligente — evita copias innecesarias)

---

## Archivos de integración en la shell

Los cambios realizados en el repositorio **SistemaOperativo** para esta integración fueron:

| Archivo                          | Cambio |
| `src/commands/backup_commands.c` | Nuevo archivo con la lógica del comando `respaldo`                   |
| `include/commands.h`             | Se agregó el prototipo `void cmd_respaldo(char **args)`              |
| `src/core/shell_loop.c`          | Se registró `"respaldo"` y `&cmd_respaldo` en las tablas de comandos |

