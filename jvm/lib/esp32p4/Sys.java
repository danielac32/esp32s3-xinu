package esp32p4;

/**
 * esp32p4.Sys — servicios del kernel desde Java.
 *
 * Permite a una app Java crear tareas (lanzar otra .class con sus
 * argumentos), matarlas, dormir/ceder y consultar el tid actual.  Se apoya
 * en la reentrancia de la VM: cada tarea Java tiene su propia instancia.
 * C: host jvm/esp32p4_sys.c, P4 device/jvm/native_sys.c.
 */
public class Sys {

    /**
     * Lanza /java/&lt;cls&gt;.class como tarea, con {@code args} en su
     * main(String[]), y espera a que termine. Devuelve 1 si ok.
     */
    public static native int run(String cls, String[] args);

    /**
     * Ejecuta /java/&lt;cls&gt;.class en la tarea ACTUAL (sin crear tarea
     * nueva): el estado JVM se guarda/restaura, asi que el llamador
     * reanuda intacto. Bloqueante. Devuelve 1 si ok.
     */
    public static native int runHere(String cls, String[] args);

    /** Mata la tarea {@code tid}. Devuelve 1 si ok. */
    public static native int kill(int tid);

    /** Duerme la tarea actual {@code ms} ms (cede la CPU). */
    public static native int sleep(int ms);

    /** Cede la CPU. Devuelve 1. */
    public static native int yield();

    /** Tid de la tarea actual. */
    public static native int tid();
}
