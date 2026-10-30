package esp32p4;

/**
 * esp32p4.LVGL — acceso a la UI LVGL v8 desde Java.
 *
 * LVGL ya esta inicializado por el kernel (device LVGL0).  Los widgets se
 * crean con las fabricas label/button/rect y se manipulan con setText/setPos/
 * setSize/setColor.  El bucle de la app debe llamar tick() cada ~5 ms y
 * consultar pollEvent()/eventWidget() para los toques.
 *
 * Nota: LVGL no es thread-safe; toda la UI se maneja desde una unica tarea
 * (la que corre el bucle de la app).  Sin GC: reusar widgets, no recrear
 * en cada frame.
 */
public class LVGL {

    /** Inicializa (tick base). LVGL ya arranco en devinit. */
    public static native void init();

    /** Crea un screen nuevo, lo carga y devuelve su handle (0 si fallo). */
    public static native int screen();

    /** Limpia el screen activo y lo rellena con color (0xRRGGBB). */
    public static native void clear(int color);

    /** Crea un label. Devuelve handle (0 si fallo). */
    public static native int label(int parent, String text, int x, int y);

    /** Crea un boton con texto centrado. Devuelve handle. */
    public static native int button(int parent, String text, int x, int y, int w, int h);

    /** Crea un rectangulo de color. Devuelve handle. */
    public static native int rect(int parent, int x, int y, int w, int h, int color);

    /** Cambia el texto de un label. */
    public static native void setText(int w, String text);

    /** Formatea en C (sin asignar Strings en Java): fmt con %d, a, b. */
    public static native void setTextFmt(int w, String fmt, int a, int b);

    /** Mueve un widget. */
    public static native void setPos(int w, int x, int y);

    /** Redimensiona un widget. */
    public static native void setSize(int w, int width, int height);

    /** Color de texto (fg) y fondo (bg), 0xRRGGBB. */
    public static native void setColor(int w, int fg, int bg);

    /** Avanza el tick de LVGL y ejecuta lv_timer_handler() (render). */
    public static native void tick();

    /** Devuelve 1 si hay un evento pendiente (y lo consume), 0 si no. */
    public static native int pollEvent();

    /** Handle del widget del ultimo evento consumido. */
    public static native int eventWidget();

    /** Borra un widget. */
    public static native void del(int w);

    /**
     * Carga una imagen raw RGB565 (w*h*2 bytes) desde path y la muestra.
     * Devuelve handle (0 si fallo). El fichero debe ser RGB565 sin cabecera.
     */
    public static native int img(int parent, String path, int x, int y, int w, int h);

    /** Ancho de un widget en px. */
    public static native int getWidth(int w);

    /** Alto de un widget en px. */
    public static native int getHeight(int w);

    /** Ancho de la pantalla activa en px. */
    public static native int screenWidth();

    /** Alto de la pantalla activa en px. */
    public static native int screenHeight();
}
