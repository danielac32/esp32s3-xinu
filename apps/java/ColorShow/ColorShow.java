/*
 * ColorShow — demuestra el LED RGB (device RGB0) desde la JVM.
 *
 * Pines (include/board.h):
 *   rojo  GPIO48
 *   verde GPIO47
 *   azul  GPIO41
 *
 * El programa entra en un bucle infinito con tres fases:
 *
 *   1. shift  : recorre la paleta color a color.
 *   2. blink  : parpadea tres veces cada color.
 *   3. fade   : interpola suavemente de un color al siguiente.
 *
 * No imprime nada dentro del bucle: la JVM no tiene GC y cada println
 * consume slots del heap.  Para pararlo, mira el PID del hilo "JVM"
 * con `ps` y ejecuta `kill <PID>` desde otra sesión (webterm/telnet).
 *
 * Compilar:
 *   make            -> build/ColorShow.class
 *   make run        -> ejecuta en la JVM del host (sin hardware)
 */
import esp32p4.RGB;
import esp32p4.HW;

public class ColorShow {

    /* Paleta 0x00RRGGBB.  Se rellena en main() para no depender de
     * inicializadores estáticos de arrays en <clinit>. */
    static int[] palette;
    static int count;

    static int red(int c)   { return (c >> 16) & 255; }
    static int green(int c) { return (c >> 8) & 255; }
    static int blue(int c)  { return c & 255; }

    static void setColor(int c) {
        RGB.set(red(c), green(c), blue(c));
    }

    static void buildPalette() {
        palette = new int[8];
        palette[0] = 0xFF0000;   /* rojo     */
        palette[1] = 0x00FF00;   /* verde    */
        palette[2] = 0x0000FF;   /* azul     */
        palette[3] = 0xFFFF00;   /* amarillo */
        palette[4] = 0x00FFFF;   /* cian     */
        palette[5] = 0xFF00FF;   /* magenta  */
        palette[6] = 0xFFFFFF;   /* blanco   */
        palette[7] = 0x000000;   /* apagado  */
        count = 8;
    }

    /* Fase 1: registro de desplazamiento de colores. */
    static void shiftPhase() {
        int i;
        for (i = 0; i < count; i = i + 1) {
            setColor(palette[i]);
            HW.delay(320);
        }
    }

    /* Fase 2: tres parpadeos por color. */
    static void blinkPhase() {
        int i;
        int k;
        for (i = 0; i < count; i = i + 1) {
            for (k = 0; k < 3; k = k + 1) {
                setColor(palette[i]);
                HW.delay(130);
                RGB.off();
                HW.delay(130);
            }
        }
    }

    /* Fase 3: fade lineal de cada color al siguiente. */
    static void fadePhase() {
        int i;
        int s;
        int steps = 16;
        for (i = 0; i < count; i = i + 1) {
            int a = palette[i];
            int next = i + 1;
            if (next >= count) next = 0;
            int b = palette[next];

            for (s = 0; s <= steps; s = s + 1) {
                int r = (red(a) * (steps - s) + red(b) * s) / steps;
                int g = (green(a) * (steps - s) + green(b) * s) / steps;
                int bl = (blue(a) * (steps - s) + blue(b) * s) / steps;
                RGB.set(r, g, bl);
                HW.delay(28);
            }
        }
    }

    public static void main(String[] args) {
        buildPalette();

        System.out.println("ColorShow: R=48 G=47 B=41");
        System.out.println("bucle shift -> blink -> fade");
        System.out.println("para parar: ps; kill <PID del hilo JVM>");

        /* Verifica que el device responde antes de entrar en el bucle. */
        RGB.set(255, 0, 0);
        HW.delay(400);
        RGB.off();
        HW.delay(200);

        int round = 0;
        while (true) {
            shiftPhase();
            blinkPhase();
            fadePhase();
            round = round + 1;
        }
    }
}
