/* StrBuild: verifica que la concatenacion deja un String REAL y usable.
 * Antes: toString no empujaba nada -> `String s = "a"+x;` hacia underflow
 * y corrompia la pila. Ahora el resultado es un String del heap que se
 * puede guardar en un local, imprimir, pasar a metodos y a natives. */
public class StrBuild {
    public static void main(String[] args) {
        // 1) concat guardado en variable (el bug del punto 1)
        String s = "score=" + 120 + " nivel=" + 3 + " ok";
        System.out.println("s = " + s);
        System.out.println("length = " + s.length());

        // 2) concat directo a println
        System.out.println("directo: " + 1 + 2 + 3);

        // 3) pasar el String concatenado a un metodo
        report(s);

        // 4) String devuelto por un metodo y luego concatenado
        String t = greet("mundo");
        System.out.println("t = " + t + " (len " + t.length() + ")");

        // 5) charAt + equals sobre Strings reales
        System.out.println("t.charAt(0) = " + t.charAt(0));
        System.out.println("t.equals(greet) = " + t.equals(greet("mundo")));

        // 6) charAt sobre el resultado de concatenar
        String u = "abc" + 123;
        System.out.println("u = " + u + ", u.charAt(2) = " + u.charAt(2));
    }

    static void report(String m) {
        System.out.println("report -> " + m);
    }

    static String greet(String name) {
        return "hola " + name + "!";
    }
}
