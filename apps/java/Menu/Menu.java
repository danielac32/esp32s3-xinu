/*
 * Menu — app interactiva de demostración para ESP32-P4.
 *
 * Ejercita el núcleo de la VM y los módulos esp32p4.*:
 *   - bucles while / for
 *   - cadena if/else-if
 *   - métodos estáticos con argumento y retorno int
 *   - recursión (fact)
 *   - arrays de int con índices (fib)
 *   - campo estático (presses, inicializado en <clinit>)
 *   - concatenación de strings
 *   - esp32p4.IO: print, println, readChar   (consola)
 *   - esp32p4.HW: delay, millis, gpioWrite   (hardware)
 *
 * Sin long/double (la VM almacena la pila como int). Consola por
 * readChar(); en el P4 es la UART.
 */
import esp32p4.HW;
import esp32p4.IO;

public class Menu {
    /* campo estático: la VM ejecuta <clinit> antes de main() */
    static int presses = 0;

    public static void main(String[] args) {
        IO.println("");
        IO.println("=== ESP32-P4 JVM app demo ===");
        IO.println("apps interactivas sobre la JVM");
        int choice = -1;
        while (choice != 0) {
            IO.println("");
            IO.println("1) blink LED");
            IO.println("2) uptime");
            IO.println("3) eco de teclas");
            IO.println("4) fib(10) con array");
            IO.println("5) fact(10) recursivo");
            IO.println("6) contador estatico");
            IO.println("0) salir");
            IO.print("> ");
            choice = readChoice();
            if (choice == 1) blink();
            else if (choice == 2) uptime();
            else if (choice == 3) echoKeys();
            else if (choice == 4) fib();
            else if (choice == 5) factDemo();
            else if (choice == 6) counter();
            else if (choice != 0) IO.println("opcion invalida");
        }
        IO.println("");
        IO.println("bye, presses=" + presses);
    }

    /* lee una tecla, salta el salto de línea, devuelve valor numérico */
    static int readChoice() {
        int c = skipNl();
        presses = presses + 1;
        if (c < 0) return 0;   /* EOF: salir para no buclear */
        return c - 48;         /* '0' = 48 */
    }

    /* lee ignorando Enter; -1 al llegar a EOF */
    static int skipNl() {
        int c = IO.readChar();
        while (c == 10 || c == 13) c = IO.readChar();
        return c;
    }

    static void blink() {
        int t0 = HW.millis();
        int i = 0;
        for (i = 0; i < 4; i = i + 1) {
            HW.gpioWrite(2, 1);
            HW.delay(50);
            HW.gpioWrite(2, 0);
            HW.delay(50);
        }
        IO.println("blink: 4 bordes en " + (HW.millis() - t0) + " ms");
    }

    static void uptime() {
        int ms = HW.millis();
        IO.println("uptime = " + ms + " ms (" + (ms / 1000) + " s)");
    }

    static void echoKeys() {
        IO.println("eco: pulsa 3 teclas (se ignora el Enter)");
        int a = skipNl();
        int b = skipNl();
        int c = skipNl();
        IO.println("codigos: " + a + " " + b + " " + c);
    }

    static void fib() {
        int[] f = new int[11];
        f[0] = 0;
        f[1] = 1;
        int i = 2;
        while (i < 11) {
            f[i] = f[i - 1] + f[i - 2];
            i = i + 1;
        }
        IO.println("fib(10) = " + f[10]);
    }

    static int fact(int n) {
        if (n <= 1) return 1;
        return n * fact(n - 1);
    }

    static void factDemo() {
        IO.println("fact(10) = " + fact(10));
    }

    static void counter() {
        presses = presses + 5;
        IO.println("presses (estatico) = " + presses);
    }
}
