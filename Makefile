# ═══════════════════════════════════════════════════════════════════
#  Makefile — Sistema de Backup Inteligente
# ═══════════════════════════════════════════════════════════════════

CC     = gcc
CFLAGS = -Wall -std=c11 -D_POSIX_C_SOURCE=200809L
LIBS   = -lyaml
TARGET = backup_app
SRCS   = main.c backupEngine/backup_engine.c config_parser.c

.PHONY: all clean log status stop reload

all: $(TARGET)
	@echo ""
	@echo "  ✓  Compilado correctamente → ./$(TARGET)"
	@echo ""
	@echo "  Correr   : ./$(TARGET) config.yaml"
	@echo "  Ver log  : make log"
	@echo "  Estado   : make status"
	@echo "  Recargar : make reload"
	@echo "  Detener  : make stop"
	@echo ""

$(TARGET): $(SRCS) backupEngine/sys_smart_copy.h
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

log:
	@tail -f /tmp/backup.log

status:
	@echo "" && cat /tmp/backup_status 2>/dev/null || \
	  echo "  El daemon no está corriendo."

stop:
	@PID=$$(cat /tmp/backup_daemon.pid 2>/dev/null); \
	 if [ -n "$$PID" ]; then \
	   kill $$PID && echo "  ✓ Daemon (PID $$PID) detenido."; \
	 else echo "  El daemon no está corriendo."; fi

reload:
	@PID=$$(cat /tmp/backup_daemon.pid 2>/dev/null); \
	 if [ -n "$$PID" ]; then \
	   kill -HUP $$PID && echo "  ✓ SIGHUP enviado — recargando config..."; \
	 else echo "  El daemon no está corriendo."; fi

clean:
	@rm -f $(TARGET)
	@rm -f /tmp/backup_daemon.pid /tmp/backup_status /tmp/backup.log
	@echo "  ✓ Limpiado."