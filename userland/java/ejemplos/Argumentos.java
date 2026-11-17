/* Argumentos.java - argumentos de la línea de órdenes, propiedades del sistema y código de salida.
 *
 *   java -cp /apps/java/ejemplos Argumentos uno dos
 *   java -cp /apps/java/ejemplos Argumentos falla     (sale con el código 7) */
public class Argumentos {
    public static void main(String[] args) {
        System.out.println("argumentos: " + args.length);
        for (int i = 0; i < args.length; i++) {
            System.out.println("arg " + (i + 1) + " = \"" + args[i] + "\"");
        }
        System.out.println("java.version = " + System.getProperty("java.version"));
        System.out.println("file.separator = " + System.getProperty("file.separator"));
        if (args.length > 0 && args[0].equals("falla")) {
            System.exit(7);
        }
    }
}
