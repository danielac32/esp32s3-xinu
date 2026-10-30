/*
 * WordQuest — la demo completa de la VM, como un juego de texto.
 *
 * Estresa la VM en todo lo que una app real necesitaria:
 *
 *  1. PUNTO 1 (concat ya resuelto): `String state = "" + score + ...`
 *     produce un String REAL del heap: se guarda en un local, se mide
 *     con .length(), se pasa a una native (IO.strLen) y se imprime.
 *  2. ANALISIS DE TEXTO (calculo puro en la VM): recorre un string char
 *     a char con IO.strLen/IO.strAt, separa palabras y cuenta vocales con
 *     bucles/ramas/arrays — sin ayuda del host.
 *  3. FICHEROS (esp32p4.FAT, estilo java.io): guardar, leer, modificar y
 *     releer una partida con writeAll/readAll/appendString.
 *  4. SONIDO ficticio (esp32p4.HW.sound) en momentos clave.
 *  5. Paso de variables a natives: String, int y int[] entran a las
 *     funciones nativas; los natives devuelven int.
 *
 * Sin long/double (la pila de la VM es int). En el P4, los natives de
 * ficheros/sonido van a los drivers; la ABI Java no cambia.
 */
import esp32p4.HW;
import esp32p4.IO;
import esp32p4.FAT;

public class WordQuest {
    public static void main(String[] args) {
        IO.println("");
        IO.println("=== WORD QUEST — prueba de la VM ===");
        IO.println("motor: JVM simple (int+float, frames, heap, natives)");
        IO.print("pulsa Enter para empezar ... ");
        IO.readChar();                       /* compuerta interactiva */

        /* escena 1: analisis de texto, calculo puro en la VM */
        IO.println("");
        IO.println("-- escena 1: analisis de texto --");
        String text = "el veloz murcielago hindu comia feliz cardillo y kiwi";
        IO.println("  texto: " + text);
        analyze(text);
        HW.sound(880, 120);                  /* chime ficticio */

        /* escena 2: partida en la SD (guardar / leer / modificar) */
        IO.println("");
        IO.println("-- escena 2: partida en la SD --");
        saveLoad();

        IO.println("");
        IO.println("=== fin de la demo ===");
    }

    /* encuentra la palabra con mas vocales recorriendo el texto */
    static void analyze(String text) {
        int n = IO.strLen(text);
        int maxCount = 0;
        int maxStart = 0;
        int maxEnd = 0;
        int wordStart = 0;
        int i, c, count;
        for (i = 0; i <= n; i = i + 1) {
            if (i == n) c = 32; else c = IO.strAt(text, i); /* 32 = espacio */
            if (c == 32) {
                count = countVowels(text, wordStart, i);
                if (count > maxCount) {
                    maxCount = count;
                    maxStart = wordStart;
                    maxEnd = i;
                }
                wordStart = i + 1;
            }
        }
        /* copiar la palabra ganadora a un buffer y mostrarla */
        int[] word = new int[maxEnd - maxStart];
        for (i = maxStart; i < maxEnd; i = i + 1)
            word[i - maxStart] = IO.strAt(text, i);
        IO.print("  palabra con mas vocales: ");
        IO.printBuf(word, word.length);
        IO.println("  (" + maxCount + " vocales)");
    }

    static int countVowels(String text, int from, int to) {
        int c = 0;
        int k;
        for (k = from; k < to; k = k + 1)
            c = c + isVowel(IO.strAt(text, k));
        return c;
    }

    static int isVowel(int ch) {
        if (ch == 97 || ch == 101 || ch == 105 || ch == 111 || ch == 117) return 1; /* a e i o u */
        if (ch == 65 || ch == 69 || ch == 73 || ch == 79 || ch == 85) return 1;     /* A E I O U */
        return 0;
    }

    static void saveLoad() {
        int score = 120;
        int level = 3;
        int lives = 5;

        /* PUNTO 1: concat guardado en variable — ahora es un String real */
        String state = "" + score + " " + level + " " + lives;
        IO.println("  estado construido: \"" + state + "\""
                   + " (length=" + state.length()
                   + ", strLen=" + IO.strLen(state) + ")");

        /* guardar con la API de ficheros estilo java.io (sobrescribe) */
        int ok = FAT.writeAll("save.dat", state + "\n");
        IO.println("  guardar score=" + score + " nivel=" + level
                   + " vidas=" + lives + " -> ok=" + ok);

        /* leer el fichero completo como String */
        String back = FAT.readAll("save.dat");
        IO.print("  leer SD:   ");
        IO.print(back);
        IO.println("(" + IO.strLen(back) + " bytes)");

        /* parsear el texto de vuelta a ints (calculo en la VM) */
        int[] parsed = new int[3];
        parseInts(back, parsed);
        IO.println("  parsear:   score=" + parsed[0]
                   + " nivel=" + parsed[1] + " vidas=" + parsed[2]);

        /* modificar: recalcular el estado y anadirlo (append) */
        int s2 = parsed[0] + 130;   /* 250 */
        int l2 = parsed[1] + 1;     /* 4   */
        int v2 = 2;
        FAT.appendString("save.dat", s2 + " " + l2 + " " + v2 + "\n");
        HW.sound(660, 80);

        /* releer el fichero modificado */
        String back2 = FAT.readAll("save.dat");
        IO.print("  releer SD: ");
        IO.print(back2);
        IO.println("(" + IO.strLen(back2) + " bytes)");
    }

    /* parsea enteros separados por espacios desde un String */
    static void parseInts(String text, int[] out) {
        int len = IO.strLen(text);
        int count = 0;
        int num = 0;
        int inNum = 0;
        int i, ch;
        for (i = 0; i < len; i = i + 1) {
            ch = IO.strAt(text, i);
            if (ch >= 48 && ch <= 57) {         /* digito '0'..'9' */
                num = num * 10 + (ch - 48);
                inNum = 1;
            } else {
                if (inNum != 0) {
                    if (count < out.length) out[count] = num;
                    count = count + 1;
                    num = 0;
                    inNum = 0;
                }
            }
        }
        if (inNum != 0 && count < out.length) {
            out[count] = num;
            count = count + 1;
        }
    }
}
