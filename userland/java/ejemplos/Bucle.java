/* Bucle.java - mide el rendimiento de la VM: un bucle de enteros, uno de flotantes y la creación de objetos.
 *
 *   java -cp /apps/java/ejemplos Bucle [millones]
 * Imprime el tiempo de cada prueba en milisegundos (System.currentTimeMillis). */
public class Bucle {
    public static void main(String[] args) {
        int n = (args.length > 0 ? Integer.parseInt(args[0]) : 5) * 1000000;
        long t0 = System.currentTimeMillis();
        long suma = 0;
        for (int i = 0; i < n; i++) {
            suma += i % 7;
        }
        long t1 = System.currentTimeMillis();
        System.out.println("bucle entero: " + n + " vueltas, suma " + suma + ", " + (t1 - t0) + " ms");
        double d = 0;
        for (int i = 1; i <= n / 10; i++) {
            d += 1.0 / i;
        }
        long t2 = System.currentTimeMillis();
        System.out.println("bucle flotante: " + (n / 10) + " vueltas, " + (t2 - t1) + " ms");
        Object[] o = new Object[1000];
        for (int i = 0; i < n / 10; i++) {
            o[i % 1000] = new int[8];
        }
        long t3 = System.currentTimeMillis();
        System.out.println("objetos: " + (n / 10) + " arrays, " + (t3 - t2) + " ms");
    }
}
