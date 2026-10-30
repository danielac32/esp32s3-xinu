/*
 * Fat — API de ficheros esp32p4.FAT (estilo java.io) sobre la "tarjeta".
 *
 * En el host la tarjeta es ./sdcard/.  Prueba: mkdir/isDirectory, escritura
 * y append por ruta, exists/length/readAll, list(String[]) y borrado.
 */
import esp32p4.FAT;

public class Fat {
    public static void main(String[] args) {
        FAT.delete("jvmtestdir/a.txt");
        FAT.rmdir("jvmtestdir");

        FAT.mkdir("jvmtestdir");
        System.out.println("mkdir=" + FAT.isDirectory("jvmtestdir"));

        FAT.writeAll("jvmtestdir/a.txt", "hola");
        FAT.appendString("jvmtestdir/a.txt", " mundo");
        System.out.println("exists=" + FAT.exists("jvmtestdir/a.txt"));
        System.out.println("len=" + FAT.length("jvmtestdir/a.txt"));
        System.out.println("text=" + FAT.readAll("jvmtestdir/a.txt"));

        String[] names = FAT.list("jvmtestdir");
        System.out.println("files=" + names.length + " first=" + names[0]);

        System.out.println("del=" + FAT.delete("jvmtestdir/a.txt"));
        System.out.println("rmdir=" + FAT.rmdir("jvmtestdir"));
        System.out.println("gone=" + FAT.exists("jvmtestdir/a.txt")
                           + FAT.isDirectory("jvmtestdir"));
    }
}
