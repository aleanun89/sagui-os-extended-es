/* Excepciones.java - try/catch/finally, excepciones propias y excepciones no capturadas.
 *
 *   java -cp /apps/java/ejemplos Excepciones          (termina bien)
 *   java -cp /apps/java/ejemplos Excepciones boom     (excepción sin capturar: sale con error) */
public class Excepciones {
    static class SaldoInsuficiente extends Exception {
        SaldoInsuficiente(String m) { super(m); }
    }

    static void sacar(int saldo, int cantidad) throws SaldoInsuficiente {
        if (cantidad > saldo) {
            throw new SaldoInsuficiente("faltan " + (cantidad - saldo));
        }
    }

    public static void main(String[] args) {
        try {
            sacar(10, 25);
        } catch (SaldoInsuficiente e) {
            System.out.println("capturada: " + e.getMessage());
        } finally {
            System.out.println("finally");
        }
        try {
            int[] a = new int[2];
            a[5] = 1;
        } catch (ArrayIndexOutOfBoundsException e) {
            System.out.println("indice fuera de rango");
        }
        try {
            Object o = "texto";
            Integer i = (Integer) o;
        } catch (ClassCastException e) {
            System.out.println("conversion invalida");
        }
        try {
            System.out.println(10 / (args.length - args.length));
        } catch (ArithmeticException e) {
            System.out.println("division por cero");
        }
        System.out.println("excepciones: terminado");
        if (args.length > 0 && args[0].equals("boom")) {
            throw new IllegalStateException("sin capturar");
        }
    }
}
