/*
 * RgbDemo — ejercita el device RGB0 desde Java.
 *
 * Pines (include/board.h):
 *   rojo GPIO48, verde GPIO47, azul GPIO41.
 *
 * Dentro de la carpeta:
 *   make        compila -> build/RgbDemo.class
 *   make run    ejecuta en la JVM del host (los natives del host no
 *               encienden hardware, pero valida bytecode/argumentos)
 */
import esp32p4.RGB;
import esp32p4.HW;

public class RgbDemo {
    static void show(String name, int r, int g, int b) {
        RGB.set(r, g, b);
        int c = RGB.get();
        System.out.println(name + " -> " + c);
        HW.delay(400);
    }

    public static void main(String[] args) {
        System.out.println("RgbDemo: GPIO48=R GPIO47=G GPIO41=B");

        show("red",     255, 0, 0);
        show("green",   0, 255, 0);
        show("blue",    0, 0, 255);
        show("yellow",  255, 255, 0);
        show("cyan",    0, 255, 255);
        show("magenta", 255, 0, 255);
        show("white",   255, 255, 255);

        RGB.off();
        System.out.println("off -> " + RGB.get());
        System.out.println("RgbDemo done");
    }
}
