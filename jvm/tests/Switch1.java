public class Switch1 {
    public static void main(String[] args) {
        int x = 2;
        String r;
        switch (x) {
        case 1: r = "one"; break;
        case 2: r = "two"; break;
        default: r = "many"; break;
        }
        System.out.println("sw=" + r);
    }
}
