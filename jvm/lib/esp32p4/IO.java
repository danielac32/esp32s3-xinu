package esp32p4;

/**
 * esp32p4.IO — consola y utilidades de texto.
 *
 * print/println escriben en la consola; readChar bloquea hasta una tecla.
 * strLen/strAt/printBuf permiten recorrer/imprimir texto (bytes) que la
 * app ya tenga.  C: host jvm/esp32p4_io.c, P4 device/jvm/native_io.c.
 */
public class IO {

    /** Escribe {@code s} sin salto de línea. */
    public static native void print(String s);

    /** Escribe {@code s} con salto de línea. */
    public static native void println(String s);

    /** Espera una tecla y devuelve su código (-1 al final de entrada). */
    public static native int readChar();

    /** Longitud de {@code s} en caracteres. */
    public static native int strLen(String s);

    /** Código del carácter i de {@code s} (-1 fuera de rango). */
    public static native int strAt(String s, int i);

    /** Imprime {@code len} códigos del buffer {@code buf} como texto. */
    public static native void printBuf(int[] buf, int len);
}
