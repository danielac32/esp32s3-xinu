/*
 * DelayDemo — test de natives de la librería HW.
 *
 * Dentro de la carpeta:
 *
 *   make        compila  -> build/DelayDemo.class
 *   make run    compila y ejecuta en la JVM del host
 *
 * Toda la lógica va en main(String[]) estático, que es el punto de
 * entrada que la VM busca (además de <clinit> para los estáticos).
 */
import esp32p4.HW;

public class DelayDemo {
    public static void main(String[] args) {
        System.out.println("ESP32-P4 JVM: standard-library test");

        int t0 = HW.millis();
        HW.delay(100);
        int dt = HW.millis() - t0;
        System.out.println("delay(100) measured = " + dt + " ms");

        HW.gpioWrite(2, 1);
        HW.gpioWrite(2, 0);
        int v = HW.gpioRead(2);
        System.out.println("gpioRead(2) = " + v);

        System.out.println("OK");
    }
}
