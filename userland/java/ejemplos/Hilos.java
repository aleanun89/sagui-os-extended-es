/* Hilos.java - hilos de Java (java.lang.Thread), synchronized, wait/notify y join.
 *
 *   java -cp /apps/java/ejemplos Hilos */
public class Hilos {
    static int contador = 0;
    static final Object cerrojo = new Object();

    static void sumar() {
        synchronized (cerrojo) {
            contador++;
        }
    }

    public static void main(String[] args) throws Exception {
        Thread[] hilos = new Thread[4];
        for (int i = 0; i < hilos.length; i++) {
            hilos[i] = new Thread(new Runnable() {
                public void run() {
                    for (int j = 0; j < 2000; j++) {
                        sumar();
                    }
                }
            });
            hilos[i].start();
        }
        for (Thread t : hilos) {
            t.join();
        }
        System.out.println("contador = " + contador);

        // productor y consumidor con wait/notify
        final int[] caja = new int[1];
        final boolean[] llena = new boolean[1];
        Thread consumidor = new Thread(new Runnable() {
            public void run() {
                synchronized (caja) {
                    while (!llena[0]) {
                        try { caja.wait(); } catch (InterruptedException e) { return; }
                    }
                    System.out.println("consumidor recibe " + caja[0]);
                }
            }
        });
        consumidor.start();
        Thread.sleep(50);
        synchronized (caja) {
            caja[0] = 42;
            llena[0] = true;
            caja.notifyAll();
        }
        consumidor.join();
        System.out.println("hilos: terminado");
    }
}
