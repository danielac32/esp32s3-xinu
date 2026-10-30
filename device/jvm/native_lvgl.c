/*
 * native_lvgl.c — natives de esp32p4.LVGL para el ESP32-S3.
 *
 * FASE 1 (JVM en consola): LVGL todavía no está portado, así que estos
 * nativos son stubs que mantienen la pila de la VM equilibrada (consumen
 * exactamente los argumentos que empuja el bytecode) y devuelven "sin UI".
 * La Fase 2 sustituirá el cuerpo por llamadas reales a LVGL sobre la TFT
 * ST7735, manteniendo estos mismos símbolos y convenciones.
 *
 * Convención de pila: el ÚLTIMO argumento está en la cima.
 */
/* Embedded Xinu, ported to ESP32-S3. */

#include <kernel.h>
#include <stdio.h>
#include <string.h>

#include "simple_jvm.h"
#include "java_lib.h"
#include "jvm_io.h"
#include <tft.h>

static void pop_text(StackFrame *stack, SimpleConstantPool *p, char *out,
                     int max)
{
    jvm_pop_text(stack, p, out, max);
}

static int lvgl_warned = 0;

static void lvgl_unavailable(void)
{
    if (!lvgl_warned)
    {
        lvgl_warned = 1;
        printf("lvgl: no compilado en este puerto (Fase 2)\r\n");
    }
}

/* init() -> 0 mientras no haya LVGL */
int esp32p4_lvgl_init(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    lvgl_unavailable();
    pushInt(stack, 0);
    return 0;
}

/* screen() -> handle (0) */
int esp32p4_lvgl_screen(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    pushInt(stack, 0);
    return 0;
}

/* clear(color) */
int esp32p4_lvgl_clear(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* label(parent, text, x, y) -> handle (0) */
int esp32p4_lvgl_label(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    char text[128];

    (void)type;
    (void)popInt(stack);                /* y  */
    (void)popInt(stack);                /* x  */
    pop_text(stack, p, text, sizeof(text));
    (void)popInt(stack);                /* parent */
    pushInt(stack, 0);
    return 0;
}

/* button(parent, text, x, y, w, h) -> handle (0) */
int esp32p4_lvgl_button(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    char text[64];

    (void)type;
    (void)popInt(stack);                /* h */
    (void)popInt(stack);                /* w */
    (void)popInt(stack);                /* y */
    (void)popInt(stack);                /* x */
    pop_text(stack, p, text, sizeof(text));
    (void)popInt(stack);                /* parent */
    pushInt(stack, 0);
    return 0;
}

/* rect(parent, x, y, w, h, color) -> handle (0) */
int esp32p4_lvgl_rect(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);                /* color  */
    (void)popInt(stack);                /* h      */
    (void)popInt(stack);                /* w      */
    (void)popInt(stack);                /* y      */
    (void)popInt(stack);                /* x      */
    (void)popInt(stack);                /* parent */
    pushInt(stack, 0);
    return 0;
}

/* setText(w, text) */
int esp32p4_lvgl_setText(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    char text[256];

    (void)type;
    pop_text(stack, p, text, sizeof(text));
    (void)popInt(stack);                /* w */
    pushInt(stack, 0);
    return 0;
}

/* setTextFmt(w, fmt, a, b) */
int esp32p4_lvgl_setTextFmt(StackFrame *stack, SimpleConstantPool *p,
                            char *type)
{
    char fmt[64];

    (void)type;
    (void)popInt(stack);                /* b */
    (void)popInt(stack);                /* a */
    pop_text(stack, p, fmt, sizeof(fmt));
    (void)popInt(stack);                /* w */
    pushInt(stack, 0);
    return 0;
}

/* setPos(w, x, y) */
int esp32p4_lvgl_setPos(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    (void)popInt(stack);
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* setSize(w, width, height) */
int esp32p4_lvgl_setSize(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    (void)popInt(stack);
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* setColor(w, fg, bg) */
int esp32p4_lvgl_setColor(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    (void)popInt(stack);
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* tick() */
int esp32p4_lvgl_tick(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    pushInt(stack, 0);
    return 0;
}

/* pollEvent() -> 0 (sin eventos) */
int esp32p4_lvgl_pollEvent(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    pushInt(stack, 0);
    return 0;
}

/* eventWidget() -> 0 */
int esp32p4_lvgl_eventWidget(StackFrame *stack, SimpleConstantPool *p,
                             char *type)
{
    (void)p;
    (void)type;
    pushInt(stack, 0);
    return 0;
}

/* img(parent, path, x, y, w, h) -> handle (0) */
int esp32p4_lvgl_img(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    char path[128];

    (void)type;
    (void)popInt(stack);                /* h */
    (void)popInt(stack);                /* w */
    (void)popInt(stack);                /* y */
    (void)popInt(stack);                /* x */
    pop_text(stack, p, path, sizeof(path));
    (void)popInt(stack);                /* parent */
    pushInt(stack, 0);
    return 0;
}

/* del(w) */
int esp32p4_lvgl_del(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* getWidth(w) -> 0 */
int esp32p4_lvgl_getWidth(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* getHeight(w) -> 0 */
int esp32p4_lvgl_getHeight(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void)p;
    (void)type;
    (void)popInt(stack);
    pushInt(stack, 0);
    return 0;
}

/* screenWidth() -> ancho del panel (px) */
int esp32p4_lvgl_screenWidth(StackFrame *stack, SimpleConstantPool *p,
                             char *type)
{
    (void)p;
    (void)type;
    pushInt(stack, TFT_WIDTH);
    return 0;
}

/* screenHeight() -> alto del panel (px) */
int esp32p4_lvgl_screenHeight(StackFrame *stack, SimpleConstantPool *p,
                              char *type)
{
    (void)p;
    (void)type;
    pushInt(stack, TFT_HEIGHT);
    return 0;
}
