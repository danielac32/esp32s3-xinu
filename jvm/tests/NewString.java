/*
 * NewString — constructores new String(): (), (String), (char[]) y (byte[]).
 * `new` deja un objeto y el `dup` una referencia al MISMO slot; el
 * constructor lo convierte en un String real (heap_init_string).
 */
public class NewString {
    public static void main(String[] args) {
        String a = new String("hola");
        String b = new String();
        char[] cs = { 'x', 'y', 'z' };
        String c = new String(cs);
        byte[] bs = { 72, 105, 33 };     /* "Hi!" */
        String d = new String(bs);
        System.out.println(a + "|" + b + "|" + c + "|" + d + "|" + a.length());
    }
}
