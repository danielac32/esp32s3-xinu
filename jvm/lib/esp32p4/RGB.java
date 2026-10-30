package esp32p4;

/**
 * esp32p4.RGB — LED RGB discreto de la placa ESP32-S3.
 *
 * Los pines y la polaridad están en include/board.h:
 *   rojo  = GPIO48
 *   verde = GPIO47
 *   azul  = GPIO41
 *
 * El cuerpo nativo (device/jvm/native_hw.c) usa el device Xinu RGB0, así
 * que esta clase también sirve para comprobar que el driver está vivo.
 *
 * Un LED normal no tiene intensidad: un canal está encendido cuando su
 * valor es distinto de cero.  Los argumentos se mantienen en 0..255 para
 * que el código Java se parezca al de la TFT/LVGL.
 */
public class RGB {

    /** Enciende los canales indicados; 0 apaga ese canal. */
    public static native void set(int r, int g, int b);

    /** Apaga los tres canales (recuerda el último color). */
    public static native void off();

    /** Devuelve el último color como 0x00RRGGBB. */
    public static native int get();
}
