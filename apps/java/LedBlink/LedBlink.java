/*
 * LedBlink — parpadeo de un LED usando la librería HW.
 *
 * Dentro de la carpeta:
 *
 *   make        compila  -> build/LedBlink.class
 *   make run    compila y ejecuta en la JVM del host
 *
 * Los 4 flancos están desplegados a mano, pero ya no hace falta: la VM
 * implementa goto, if*, if_icmp*, tableswitch y lookupswitch, así que
 * serviría un bucle for normal.
 */
import esp32p4.HW;

public class LedBlink {
    public static void main(String[] args) {
        System.out.println("LedBlink via JVM natives (pin 2)");

        int t0 = HW.millis();
        HW.gpioWrite(2, 1);
        HW.delay(100);
        HW.gpioWrite(2, 0);
        HW.delay(100);
        HW.gpioWrite(2, 1);
        HW.delay(100);
        HW.gpioWrite(2, 0);

        int t = HW.millis() - t0;
        System.out.println("4 edges done in " + t + " ms");
    }
}
