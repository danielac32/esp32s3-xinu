package esp32p4;

/**
 * esp32p4.HW — acceso de bajo nivel al hardware del ESP32-P4.
 *
 * Un Java normal no "programa hardware": esto es la capa fina que un
 * dispositivo concreto usa por debajo.  Cada método es {@code native};
 * el cuerpo C vive en la plataforma (host: jvm/esp32p4_hw.c,
 * P4: device/jvm/native_hw.c).
 */
public class HW {

    /** Espera {@code ms} milisegundos (cede la CPU). */
    public static native void delay(int ms);

    /** Milisegundos desde el arranque. */
    public static native int millis();

    /** Escribe {@code val} (0/1) en el GPIO {@code pin}. */
    public static native void gpioWrite(int pin, int val);

    /** Lee el GPIO {@code pin} (0/1). */
    public static native int gpioRead(int pin);

    /** Sonido ficticio en {@code freq} Hz durante {@code ms} ms. */
    public static native void sound(int freq, int ms);
}
