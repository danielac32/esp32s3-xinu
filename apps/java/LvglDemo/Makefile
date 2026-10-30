# ─────────────────────────────────────────────────────────────────────────
# LvglDemo — demostracion de LVGL v8 en el ESP32-S3
# ─────────────────────────────────────────────────────────────────────────

APP      = LvglDemo
JAVAC   ?= javac
JVM     ?= ../../../jvm/jvm
BUILD   ?= build
RELEASE  = 8

LIB_DIR  = ../../../jvm/lib
LIB_SRC  = $(LIB_DIR)/esp32p4/HW.java $(LIB_DIR)/esp32p4/LVGL.java $(LIB_DIR)/esp32p4/Sys.java

.PHONY: all run clean help

all: $(BUILD)/$(APP).class
	@echo "  [ok]    $(APP) -> $(BUILD)/$(APP).class"

$(BUILD)/$(APP).class: $(APP).java $(LIB_SRC)
	@command -v $(JAVAC) >/dev/null 2>&1 || { \
		echo "ERROR: '$(JAVAC)' no está en el PATH."; \
		echo "       Instala un JDK:  dnf install java-21-openjdk-devel"; \
		exit 1; }
	@mkdir -p $(BUILD)
	@$(JAVAC) --release $(RELEASE) -Xlint:-options -d $(BUILD) $(LIB_SRC)
	@$(JAVAC) --release $(RELEASE) -Xlint:-options -cp $(BUILD) -d $(BUILD) $(APP).java

run: $(BUILD)/$(APP).class
	@test -x $(JVM) || { \
		echo "ERROR: la VM no está compilada. Ejecuta:"; \
		echo "       make -C ../../../jvm"; \
		exit 1; }
	@$(JVM) $(BUILD)/$(APP).class

clean:
	@$(RM) -r $(BUILD)

help:
	@echo "$(APP)"
	@echo "  make        compila -> $(BUILD)/$(APP).class"
	@echo "  make run    compila y ejecuta en la JVM"
	@echo "  make clean  borra $(BUILD)/"
