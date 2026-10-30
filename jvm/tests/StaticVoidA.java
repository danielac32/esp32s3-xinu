public class StaticVoidA {
    public static void main(String[] args) {
        ping();
        System.out.println("main done");
    }
    static void ping() { System.out.println("helper ran"); }
}
