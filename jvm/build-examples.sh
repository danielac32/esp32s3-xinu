#!/bin/bash
# ---------------------------------------------------------------------------
# Compila las librerías Java (esp32p4.*) y los ejemplos del ESP32-S3.
#
#   ./jvm/build-examples.sh              compila y deja todo en jvm/dist/java
#   ./jvm/build-examples.sh --to DIR     además copia el contenido a DIR
#                                        (p. ej. el punto de montaje de la SD)
#
# El árbol que se genera es exactamente lo que espera el comando `java` del
# shell: las clases en /java y las librerías en /java/esp32p4.
#
#   jvm/dist/java/Menu.class
#   jvm/dist/java/esp32p4/HW.class
#   ...
#
# Los .java son los mismos que usa el ESP32-P4 (jvm/lib y jvm/tests del SDK
# del P4, apps/java/*), de modo que las clases compiladas aquí corren sin
# cambios en ambas máquinas.
# ---------------------------------------------------------------------------
set -e
cd "$(dirname "$0")" || exit 1

TOPDIR=$(cd .. && pwd)
BUILD=$PWD/dist/java
JAVAC=${JAVAC:-javac}
RELEASE=8

# Clases de librería que se compilan.  No hay PMIC en el S3: ni el native
# (native_pmic.c no existe) ni la clase Java (esp32p4/PMIC.java no se copió).
LIB_JAVA="HW IO FAT Sys LVGL Net RGB"

# Sondas que NO se compilan todavía:
#   SysTest   usa Sys.run(), que crea una tarea Xinu con otra instancia de la
#             VM.  Está sin desarrollar/probar en el S3 (y devuelve 1 aunque
#             la clase no exista).  Ver nota en jvm/tests/SysTest.java.
#   Workspace usa LVGL (fase 2) y ademas importa esp32p4.PMIC, que en el S3
#             no existe.
#   Tetris    usa LVGL -> fase 2.
SKIP_TESTS="SysTest Workspace Tetris"

DEST=""
if [ "$1" = "--to" ]; then
    DEST="$2"
    [ -n "$DEST" ] || { echo "falta el directorio destino"; exit 1; }
fi

command -v "$JAVAC" >/dev/null 2>&1 || {
    echo "ERROR: '$JAVAC' no está en el PATH (dnf install java-21-openjdk-devel)"
    exit 1
}

rm -rf "$BUILD"
mkdir -p "$BUILD"

echo "== librerías esp32p4 =="
for c in $LIB_JAVA; do
    "$JAVAC" --release $RELEASE -Xlint:-options -d "$BUILD" "lib/esp32p4/$c.java"
    echo "   $c.java"
done

echo "== sondas de la JVM =="
for f in tests/*.java; do
    name=$(basename "$f" .java)
    skip=""
    for s in $SKIP_TESTS; do
        [ "$name" = "$s" ] && skip=1
    done
    if [ -n "$skip" ]; then
        echo "   $name (omitida)"
        continue
    fi
    "$JAVAC" --release $RELEASE -Xlint:-options -sourcepath . \
             -cp "$BUILD" -d "$BUILD" "$f"
    echo "   $name"
done

echo "== aplicaciones =="
for app in "$TOPDIR"/apps/java/*/; do
    name=$(basename "$app")
    [ -f "$app/$name.java" ] || continue
    make -s -C "$app" >/dev/null
    cp "$app/build/$name.class" "$BUILD/"
    echo "   $name"
done

echo
echo "clases en $BUILD:"
ls -l "$BUILD"/*.class | awk '{printf "   %-24s %6d B\n", $9, $5}' | sed "s|$BUILD/||"

if [ -n "$DEST" ]; then
    [ -d "$DEST" ] || { echo "ERROR: '$DEST' no existe"; exit 1; }
    mkdir -p "$DEST/java"
    cp -r "$BUILD"/. "$DEST/java/"
    echo
    echo "copiado a $DEST/java"
fi
