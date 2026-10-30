/*
 * ImgDemo — carga imagenes desde la SD y las muestra con LVGL v8.
 *
 * Demuestra los tres formatos soportados por el native LVGL.img():
 *   /img/test.rgb565  RGB565 crudo (w*h*2 bytes, sin cabecera)
 *   /img/test.png     PNG (lodepng)
 *   /img/test.jpg     JPEG (tjpgd / SJPEG)
 *
 * Los ficheros los genera tools/make_sd_assets.py.  El bucle rota entre
 * los tres formatos cada ~2.4 s y reutiliza el slot de imagen (LVGL.del
 * libera el buffer de PSRAM).
 *
 * Ejecutar: java ImgDemo
 */
import esp32p4.LVGL;
import esp32p4.HW;

public class ImgDemo {
    private static final String[] FILES = {
        "/img/test.rgb565",
        "/img/test.png",
        "/img/test.jpg"
    };
    private static final String[] NAMES = {
        "RGB565 crudo",
        "PNG (lodepng)",
        "JPEG (tjpgd)"
    };
    private static final int W = 128;
    private static final int H = 80;

    public static void main(String[] args) {
        LVGL.init();

        int scr = LVGL.screen();
        LVGL.clear(0x101820);

        int title = LVGL.label(scr, "Imagenes desde SD", 4, 2);
        LVGL.setColor(title, 0xFFFFFF, 0x101820);

        int info = LVGL.label(scr, "", 4, 106);
        LVGL.setColor(info, 0x00FF88, 0x101820);

        int img = 0;
        int i = 0;

        while (true) {
            if (img != 0) {
                LVGL.del(img);
                img = 0;
            }

            LVGL.setText(info, NAMES[i] + "  " + W + "x" + H);
            img = LVGL.img(scr, FILES[i], 0, 20, W, H);
            if (img == 0) {
                LVGL.setText(info, NAMES[i] + ": ERROR (falta el fichero?)");
            }
            System.out.println("ImgDemo: " + NAMES[i] + " handle=" + img);

            /* ~2.4 s a 60 fps */
            for (int f = 0; f < 150; f++) {
                LVGL.tick();
                HW.delay(16);
            }

            i = (i + 1) % FILES.length;
        }
    }
}
