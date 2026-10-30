#include <device.h>
#include <conf.h>
#include <rgb.h>
/*
 * native_hw.c — la "librería" esp32p4.HW implementada para el ESP32-S3.
 *
 * (La clase Java sigue llamándose "esp32p4.HW" para que las apps .class del
 * SDK del P4 corran sin recompilar; sólo cambia el cuerpo nativo.)
 *
 * Solo hardware: tiempo (delay/millis), gpio y sonido.  La consola/texto
 * vive en native_io.c y los ficheros en native_fat.c.  En el host lo
 * implementa jvm/esp32p4_hw.c.
 *
 * Convención de pila (la misma que el host): los argumentos se empujaron de
 * izquierda a derecha, así que el ÚLTIMO está en la cima; el valor de
 * retorno se empuja de vuelta.
 *
 * gpio/sound quedan como traza (printf) a la espera de fijar pines/canal.
 */
/* Embedded Xinu, Copyright (C) 2026.  All rights reserved. */

#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>
#include <clock.h>

#include "simple_jvm.h"
#include "java_lib.h"

/* ---- Math.random --------------------------------------------------- */

/* double en [0,1).  El RNG de Xinu (rand) sembrado con el reloj. */
int java_lang_math_random(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void) p;
    (void) type;
    srand((unsigned int) clkcount());
    pushDouble(stack, (double) rand() / (double) RAND_MAX);
    return 0;
}

/* ---- tiempo -------------------------------------------------------- */

int esp32p4_hw_delay(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    int ms = popInt(stack);

    (void) p;
    (void) type;
    if (ms > 0)
    {
        sleep((uint) ms);
    }
    return 0;
}

int esp32p4_hw_millis(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void) p;
    (void) type;
    /* Milisegundos desde el arranque.  En este puerto CLKTICKS_PER_SEC es
     * 1000, así que clktime son segundos y clkticks las marcas dentro del
     * segundo (una marca = un milisegundo). */
    pushInt(stack, (int)(clktime * CLKTICKS_PER_SEC + clkticks));
    return 0;
}

/* ---- gpio / sonido (traza) ---------------------------------------- */

int esp32p4_hw_gpioWrite(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    int val = popInt(stack);
    int pin = popInt(stack);

    (void) p;
    (void) type;
    printf("[native] gpioWrite(pin=%d, val=%d)\n", pin, val);
    return 0;
}

int esp32p4_hw_gpioRead(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    int pin = popInt(stack);

    (void) p;
    (void) type;
    printf("[native] gpioRead(pin=%d) -> 0\n", pin);
    pushInt(stack, 0);
    return 0;
}

int esp32p4_hw_sound(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    int ms = popInt(stack);
    int freq = popInt(stack);

    (void) p;
    (void) type;
    printf("[native] sound(freq=%d, ms=%d)\n", freq, ms);
    return 0;
}


/* ---- esp32p4.RGB: device RGB0 -------------------------------------- */

int esp32p4_rgb_set(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    int b = popInt(stack);
    int g = popInt(stack);
    int r = popInt(stack);
    uint32_t colour;

    (void) p;
    (void) type;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    colour = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    control(RGB0, RGB_CTRL_SET, (long)colour, 0);
    return 0;
}

int esp32p4_rgb_off(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void) stack;
    (void) p;
    (void) type;
    control(RGB0, RGB_CTRL_OFF, 0, 0);
    return 0;
}

int esp32p4_rgb_get(StackFrame *stack, SimpleConstantPool *p, char *type)
{
    (void) p;
    (void) type;
    pushInt(stack, (int)control(RGB0, RGB_CTRL_GET, 0, 0));
    return 0;
}
