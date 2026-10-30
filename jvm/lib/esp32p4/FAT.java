package esp32p4;

/**
 * esp32p4.FAT — acceso a la tarjeta microSD (FAT32).
 *
 * La API sigue el estilo de java.io (RandomAccessFile / File), adaptada a
 * esta VM: sin clase File (no hay class loader) se usa un handle entero
 * devuelto por open().  C: host jvm/esp32p4_fat.c (sobre ./sdcard),
 * P4 device/jvm/native_fat.c (sobre fl_* de fatfs/).
 *
 * Rutas absolutas desde la raíz de la tarjeta, p.ej. "/data/a.txt".
 */
public class FAT {

    /* ---- RandomAccessFile ----------------------------------------- */

    /** Abre un fichero. mode: "r","w","a","r+","w+","a+". Devuelve handle (>0) o 0. */
    public static native int open(String path, String mode);

    /** Cierra el handle. Devuelve 1 si ok. */
    public static native int close(int h);

    /** Lee hasta {@code len} bytes en {@code buf}; devuelve los leídos. */
    public static native int read(int h, byte[] buf, int len);

    /** Escribe hasta {@code len} bytes de {@code buf}; devuelve los escritos. */
    public static native int write(int h, byte[] buf, int len);

    /** Posiciona el cursor en {@code off}. Devuelve 1 si ok. */
    public static native int seek(int h, int off);

    /** Posición actual del cursor. */
    public static native int tell(int h);

    /** 1 si el cursor está al final. */
    public static native int eof(int h);

    /** Lee una línea (sin el '\\n'); null/"" al final. */
    public static native String readLine(int h);

    /** Escribe el texto {@code s}; devuelve los bytes escritos. */
    public static native int writeString(int h, String s);

    /* ---- atajos (toda la ruta) ------------------------------------ */

    /** Lee todo el fichero como texto ("" si no existe). */
    public static native String readAll(String path);

    /** Crea/sobrescribe {@code path} con {@code s}. Devuelve 1 si ok. */
    public static native int writeAll(String path, String s);

    /** Añade {@code s} al final de {@code path}. Devuelve 1 si ok. */
    public static native int appendString(String path, String s);

    /* ---- estilo File ---------------------------------------------- */

    /** 1 si el fichero/directorio existe. */
    public static native int exists(String path);

    /** 1 si {@code path} es un directorio. */
    public static native int isDirectory(String path);

    /** Tamaño en bytes (0 si no existe). */
    public static native int length(String path);

    /** Borra un fichero. Devuelve 1 si ok. */
    public static native int delete(String path);

    /** Crea el directorio {@code path}. Devuelve 1 si ok. */
    public static native int mkdir(String path);

    /** Borra el directorio {@code path}. Devuelve 1 si ok. */
    public static native int rmdir(String path);

    /** Lista los nombres de {@code dir} (array vacío si no existe). */
    public static native String[] list(String dir);
}
