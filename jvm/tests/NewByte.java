/*
 * NewByte — byte[] real (HEAP_KIND_BARRAY): `new byte[n]` reserva n BYTES
 * (no n ints) y baload/bastore usan el byte bajo; baload devuelve 0..255.
 */
public class NewByte {
    public static void main(String[] args) {
        byte[] b = new byte[4];
        int i;
        for (i = 0; i < b.length; i++) {
            b[i] = (byte) (i * 60);      /* 0, 60, 120, (byte)180 */
        }
        System.out.println("len=" + b.length + " b0=" + b[0]
                           + " b2=" + b[2] + " b3=" + b[3]);
        b[0] = (byte) 255;
        System.out.println("b0=" + b[0]);
    }
}
