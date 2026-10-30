/*
 * Workspace — escritorio Java sobre LVGL.
 *
 * Muestra arriba: bateria, hora (uptime) y señal (placeholder).
 * Abajo: grid de iconos, uno por cada .class de /java.
 * Al tocar un icono ejecuta la app con Sys.runHere() (in-process) y
 * al volver redibuja el escritorio.
 */
import esp32p4.LVGL;
import esp32p4.PMIC;
import esp32p4.HW;
import esp32p4.Sys;
import esp32p4.FAT;
import esp32p4.Net;

public class Workspace {
    static int screen;
    static int batteryLabel;
    static int timeLabel;
    static int signalLabel;
    static int[] icons;
    static String[] apps;

    static void drawTopBar() {
        int sw = LVGL.screenWidth();
        int bar = LVGL.rect(screen, 0, 0, sw, 24, 0x222244);
        batteryLabel = LVGL.label(bar, "bat:--%", 4, 4);
        LVGL.setColor(batteryLabel, 0xFFFFFF, 0x222244);
        timeLabel = LVGL.label(bar, "0s", sw/2 - 20, 4);
        LVGL.setColor(timeLabel, 0xFFFFFF, 0x222244);
        signalLabel = LVGL.label(bar, "WiFi", sw - 80, 4);
        LVGL.setColor(signalLabel, 0xFFFFFF, 0x222244);
    }

    static void updateTopBar() {
        int pct = PMIC.batPercent();
        LVGL.setTextFmt(batteryLabel, "bat:%d%%", pct, 0);
        int sec = HW.millis() / 1000;
        LVGL.setTextFmt(timeLabel, "%ds", sec, 0);
        if (Net.isConnected() == 1) LVGL.setText(signalLabel, "WiFi:ok");
        else LVGL.setText(signalLabel, "WiFi:--");
    }

    static void drawIcons() {
        apps = FAT.list("/java");
        if (apps == null) apps = new String[0];
        icons = new int[apps.length];
        int sw = LVGL.screenWidth();
        int cols = 4;
        int iw = 60, ih = 60, gap = 10;
        int x0 = (sw - cols*(iw+gap) + gap) / 2;
        int y0 = 40;
        for (int i = 0; i < apps.length; i++) {
            int col = i % cols;
            int row = i / cols;
            int x = x0 + col*(iw+gap);
            int y = y0 + row*(ih+gap);
            String name = apps[i];
            if (name.endsWith(".class")) name = name.substring(0, name.length()-6);
            // intenta icono raw RGB565 (/icons/<name>.rgb565); si no, boton con texto
            int ic = LVGL.img(screen, "/icons/" + name + ".rgb565", x, y, 48, 48);
            if (ic == 0) {
                icons[i] = LVGL.button(screen, name, x, y, iw, ih);
                LVGL.setColor(icons[i], 0xFFFFFF, 0x333366);
            } else {
                icons[i] = ic;
                int lbl = LVGL.label(screen, name, x, y + 50);
                LVGL.setColor(lbl, 0xFFFFFF, 0x111122);
            }
        }
    }

    static void redraw() {
        LVGL.clear(0x111122);
        screen = LVGL.screen();
        drawTopBar();
        drawIcons();
    }

    public static void main(String[] args) {
        LVGL.init();
        redraw();

        while (true) {
            updateTopBar();
            int ev = LVGL.pollEvent();
            if (ev != 0) {
                int w = LVGL.eventWidget();
                for (int i = 0; i < apps.length; i++) {
                    if (w == icons[i]) {
                        String name = apps[i];
                        if (name.endsWith(".class")) name = name.substring(0, name.length()-6);
                        LVGL.clear(0x000000);
                        String[] a = new String[0];
                        Sys.runHere(name, a);
                        redraw();
                    }
                }
            }
            LVGL.tick();
            Sys.sleep(16);
        }
    }
}