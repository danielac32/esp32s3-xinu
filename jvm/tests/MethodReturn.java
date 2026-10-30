public class MethodReturn {
    public static void main(String[] args) {
        int r = add(2, 3);
        System.out.println("add=" + r);
    }
    static int add(int a, int b) { return a + b; }
}
