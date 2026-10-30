/*
 * Stdlib — arrays (int[]/byte[]/String[]) + libreria estandar.
 *
 * int[] con inicializador, byte[] (newarray byte + bastore/baload),
 * String[] (anewarray/aastore/aaload), Integer.parseInt/toString,
 * String.valueOf, Math.abs/max/min y metodos de String.
 */
public class Stdlib {
    public static void main(String[] args) {
        int[] a = new int[1];
        a[0] = 7;
        int[] b = { 3, 4, 5 };
        byte[] by = new byte[3];
        by[0] = 65;
        by[1] = 66;
        by[2] = 67;
        String[] s = new String[2];
        s[0] = "hi";
        s[1] = "there";

        System.out.println("a0=" + a[0] + " b1=" + b[1]
                           + " by0=" + by[0] + " by2=" + by[2] + " s1=" + s[1]);

        System.out.println("parse=" + Integer.parseInt("123")
                           + " tostr=" + Integer.toString(45));

        System.out.println("v=" + String.valueOf(9) + ","
                           + String.valueOf(1.5f) + "," + String.valueOf(true));

        System.out.println("abs=" + Math.abs(-4) + " max=" + Math.max(2, 9)
                           + " min=" + Math.min(2, 9));

        String t = "  Hola Mundo  ";
        System.out.println("trim=[" + t.trim() + "] upper=" + t.trim().toUpperCase()
                           + " lower=" + t.trim().toLowerCase());

        System.out.println("sub=" + "abcdef".substring(2, 5)
                           + " idx=" + "abcdef".indexOf("cd")
                           + " sw=" + "abcdef".startsWith("abc")
                           + " ew=" + "abcdef".endsWith("def"));
    }
}
