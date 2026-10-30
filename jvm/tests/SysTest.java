/*
 * SysTest — esp32p4.Sys desde Java.
 *
 * En el host no hay tareas Xinu: Sys.run() ejecuta la clase en linea (una
 * instancia de VM anidada) y devuelve su exito.  En el P4 crea una tarea.
 * Aqui se lanza Args con dos argumentos y se comprueba el retorno.
 */
import esp32p4.Sys;

public class SysTest {
    public static void main(String[] args) {
        System.out.println("before");
        String[] a = new String[] { "uno", "dos" };
        int ok = Sys.run("Args", a);
        System.out.println("after ok=" + ok);
    }
}
