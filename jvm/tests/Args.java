/*
 * Args — main(String[]) con argumentos de la linea de comandos.
 *
 * Ejercita los opcodes de arrays de referencias: arraylength (args.length),
 * aaload (args[i]), anewarray (new String[n]) y aastore (copy[i] = args[i]).
 */
public class Args {
    public static void main(String[] args) {
        System.out.println("n=" + args.length);
        for (int i = 0; i < args.length; i++)
            System.out.println(i + ":" + args[i]);

        String[] copy = new String[args.length];
        for (int i = 0; i < args.length; i++)
            copy[i] = args[i];
        System.out.println("copy0=" + (args.length > 0 ? copy[0] : "-"));
    }
}
