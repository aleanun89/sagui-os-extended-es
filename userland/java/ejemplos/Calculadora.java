import sagui.gui.*;

/**
 * Calculadora.java - una calculadora con ventana (biblioteca sagui.gui, D376).
 *
 *   java -cp /apps/java/ejemplos Calculadora
 *
 * Usa Ventana, Etiqueta (la pantalla), Boton y Accion. Las cuatro operaciones, punto decimal,
 * C (borrar) y el teclado: los dígitos y + - * / = también funcionan escritos.
 */
public class Calculadora {
    static double acumulado = 0;
    static String operacion = null;
    static StringBuilder entrada = new StringBuilder();
    static boolean nuevo = true;
    static Etiqueta pantalla;

    static String formato(double v) {
        if (v == Math.floor(v) && Math.abs(v) < 1e12) {
            return String.valueOf((long) v);
        }
        return String.valueOf(v);
    }

    static void mostrar(String t) {
        pantalla.texto(t);
    }

    static void calcular() {
        if (operacion != null && entrada.length() > 0) {
            double b = Double.parseDouble(entrada.toString());
            if (operacion.equals("+")) acumulado += b;
            else if (operacion.equals("-")) acumulado -= b;
            else if (operacion.equals("*")) acumulado *= b;
            else if (operacion.equals("/")) acumulado = b == 0 ? Double.NaN : acumulado / b;
        } else if (entrada.length() > 0) {
            acumulado = Double.parseDouble(entrada.toString());
        }
        entrada.setLength(0);
    }

    static void pulsar(String t) {
        if (t.equals("C")) {
            acumulado = 0; operacion = null; entrada.setLength(0);
            mostrar("0");
        } else if (t.equals("+") || t.equals("-") || t.equals("*") || t.equals("/")) {
            calcular();
            operacion = t;
            mostrar(formato(acumulado));
        } else if (t.equals("=")) {
            calcular();
            operacion = null;
            mostrar(formato(acumulado));
            System.out.println("resultado = " + formato(acumulado));   // también por la consola (la prueba lo lee)
        } else {
            if (t.equals(".") && entrada.indexOf(".") >= 0) return;
            if (entrada.length() < 14) entrada.append(t);
            mostrar(entrada.toString());
        }
    }

    public static void main(String[] args) {
        final Ventana v = new Ventana("Calculadora", 232, 292);
        pantalla = new Etiqueta("0", 12, 12);
        pantalla.fuente = Fuente.SISTEMA;
        v.agregar(new Lienzo(8, 8, 216, 32) {
            protected void pintar(Grafico g) {
                g.color(Colores.NEGRO);
                g.rect(0, 0, ancho, alto);
            }
        });
        v.agregar(pantalla);
        String[] teclas = { "7", "8", "9", "/", "4", "5", "6", "*", "1", "2", "3", "-", "0", ".", "=", "+" };
        for (int i = 0; i < teclas.length; i++) {
            final String t = teclas[i];
            v.agregar(new Boton(t, 8 + (i % 4) * 56, 52 + (i / 4) * 48, 50, 42, new Accion() {
                public void ejecutar(Componente origen) { pulsar(t); }
            }));
        }
        v.agregar(new Boton("C", 8, 244, 106, 40, new Accion() {
            public void ejecutar(Componente origen) { pulsar("C"); }
        }));
        v.agregar(new Boton("Salir", 118, 244, 106, 40, new Accion() {
            public void ejecutar(Componente origen) { v.cerrar(); }
        }));
        v.ejecutar();
    }
}
