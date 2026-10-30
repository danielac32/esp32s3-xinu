package esp32p4;

public class Net {
    /** IP actual como String, o null si no hay conexion. */
    public static native String ip();
    /** 1 si hay IP asignada. */
    public static native int isConnected();
}
