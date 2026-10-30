/*
 * Tetris — ejemplo para dimensionar natives y recursividad.
 * Usa arrays 1D (la VM no soporta int[][]).
 */
import esp32p4.LVGL;
import esp32p4.HW;
import esp32p4.Sys;

public class Tetris {
    static final int COLS = 10;
    static final int ROWS = 20;
    static final int CELL = 16;
    static int[] grid = new int[ROWS * COLS];
    static int score = 0;
    static int level = 1;
    static int curX = 3, curY = 0, curRot = 0, curType = 0;

    // pieza cuadrado: offsets (dx,dy) x4, 4 rotaciones iguales
    static int[] piece = new int[] { 0,0, 1,0, 0,1, 1,1 };

    static int idx(int r, int c) { return r * COLS + c; }

    static boolean collides(int x, int y) {
        for (int i = 0; i < 4; i += 2) {
            int cx = x + piece[i];
            int cy = y + piece[i+1];
            if (cx < 0 || cx >= COLS || cy >= ROWS) return true;
            if (cy >= 0 && grid[idx(cy,cx)] != 0) return true;
        }
        return false;
    }

    static void lockPiece() {
        for (int i = 0; i < 4; i += 2) {
            int cx = curX + piece[i];
            int cy = curY + piece[i+1];
            if (cy >= 0 && cy < ROWS && cx >= 0 && cx < COLS)
                grid[idx(cy,cx)] = 1;
        }
    }

    static void clearLines() {
        for (int r = ROWS-1; r >= 0; r--) {
            boolean full = true;
            for (int c = 0; c < COLS; c++) if (grid[idx(r,c)] == 0) { full = false; break; }
            if (full) {
                score += 100;
                for (int rr = r; rr > 0; rr--)
                    for (int c = 0; c < COLS; c++)
                        grid[idx(rr,c)] = grid[idx(rr-1,c)];
                for (int c = 0; c < COLS; c++) grid[idx(0,c)] = 0;
                r++;
            }
        }
    }

    static void render(int screen) {
        LVGL.clear(0x111122);
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (grid[idx(r,c)] != 0)
                    LVGL.rect(screen, c*CELL, r*CELL, CELL-1, CELL-1, 0x00FF00);
        for (int i = 0; i < 4; i += 2) {
            int cx = curX + piece[i];
            int cy = curY + piece[i+1];
            if (cy >= 0)
                LVGL.rect(screen, cx*CELL, cy*CELL, CELL-1, CELL-1, 0xFF0000);
        }
    }

    public static void main(String[] args) {
        LVGL.init();
        int screen = LVGL.screen();
        LVGL.clear(0x111122);
        int scoreLabel = LVGL.label(screen, "Score: 0", 10, 10);
        int levelLabel = LVGL.label(screen, "Level: 1", 10, 30);

        int lastDrop = HW.millis();
        int dropInterval = 500;

        for (int frame = 0; frame < 200; frame++) {
            LVGL.tick();
            int now = HW.millis();
            if (now - lastDrop > dropInterval) {
                lastDrop = now;
                if (!collides(curX, curY+1)) {
                    curY++;
                } else {
                    lockPiece();
                    clearLines();
                    curX = 3; curY = 0;
                    if (collides(curX, curY)) {
                        LVGL.setText(scoreLabel, "GAME OVER");
                        break;
                    }
                }
            }
            render(screen);
            LVGL.setTextFmt(scoreLabel, "Score: %d", score, 0);
            LVGL.setTextFmt(levelLabel, "Level: %d", level, 0);
            Sys.sleep(16);
        }
    }
}