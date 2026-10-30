/*
 * LvglDemo — demostracion de LVGL v8 en el ESP32-S3.
 *
 * Crea labels, un boton, un rectangulo y una imagen PNG desde la SD, y
 * refresca el contador de frames en un bucle de tick().
 *
 * Compilar: javac --release 8 -d build LvglDemo.java
 * Ejecutar: java LvglDemo
 */
import esp32p4.LVGL;
import esp32p4.HW;

public class LvglDemo {
    public static void main(String[] args) {
        LVGL.init();

        int scr = LVGL.screen();
        LVGL.clear(0x000033);

        int title = LVGL.label(scr, "Xinu S3 + LVGL8", 8, 4);
        LVGL.setColor(title, 0xFFFFFF, 0x000033);

        int btn = LVGL.button(scr, "Click me", 24, 30, 80, 26);
        LVGL.setColor(btn, 0xFFFFFF, 0x0066CC);

        int rect = LVGL.rect(scr, 8, 64, 112, 12, 0x00FF00);

        /* PNG 128x80 desde /img/test.png (generado por make_sd_assets.py).
         * Si no existe, img devuelve 0 y el resto sigue funcionando. */
        int img = LVGL.img(scr, "/img/test.png", 0, 78, 128, 80);
        System.out.println("LvglDemo: png handle=" + img);

        System.out.println("LvglDemo: screenWidth=" + LVGL.screenWidth()
                           + " screenHeight=" + LVGL.screenHeight());

        int frame = 0;
        while (true) {
            LVGL.tick();
            frame++;

            if (frame % 60 == 0) {
                LVGL.setTextFmt(title, "Frame %d", frame, 0);
                int x = 8 + (frame / 60) % 96;
                LVGL.setPos(rect, x, 64);
            }

            HW.delay(16);
        }
    }
}
