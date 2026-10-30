/* FloatTest: soporte de float en la VM — constants (fconst/ldc), locales
 * (fload/fstore), aritmetica (fadd/fsub/fmul/fdiv/fneg), comparacion
 * (fcmpl/fcmpg + ramas), conversiones (i2f/f2i), argumento+retorno de
 * metodo float, y campos float (estatico e instancia). */
public class FloatTest {
    static float scale = 2.5f;      // campo estatico float
    float offset = 0.5f;            // campo de instancia float

    public static void main(String[] args) {
        // constants
        float z = 0.0f;             // fconst_0
        float one = 1.0f;           // fconst_1
        float two = 2.0f;           // fconst_2
        float a = 1.5f;             // ldc float
        System.out.println("z=" + z + " one=" + one + " two=" + two);
        System.out.println("a = " + a);

        // aritmetica
        float sum = a + two;
        float prod = a * two;
        float quot = a / two;
        float neg = -a;
        System.out.println("sum = " + sum);
        System.out.println("prod = " + prod);
        System.out.println("quot = " + quot);
        System.out.println("neg = " + neg);
        System.out.println(prod);           // print float directo

        // comparacion
        if (a < two) System.out.println("a < two");
        else System.out.println("a >= two");
        if (prod > 3.0f) System.out.println("prod > 3.0");
        else System.out.println("prod <= 3.0");

        // conversiones int <-> float
        int n = (int) (a * 10);     // f2i
        float fromInt = 7;          // i2f
        System.out.println("n = " + n + ", fromInt = " + fromInt);

        // argumento + retorno float
        float halved = half(prod);
        System.out.println("half(prod) = " + halved);

        // campo estatico float
        System.out.println("scale = " + scale);
        scale = scale * 2;
        System.out.println("scale*2 = " + scale);

        // campo de instancia float
        FloatTest t = new FloatTest();
        System.out.println("offset = " + t.offset);
        t.offset = t.offset + 1.0f;
        System.out.println("offset+1 = " + t.offset);
    }

    static float half(float x) {
        return x / 2.0f;
    }
}
