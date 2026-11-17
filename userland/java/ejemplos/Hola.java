/* Hola.java - el primer programa de Java en SaguiOS (D376).
 *
 * Se ejecuta con:  java -cp /apps/java/ejemplos Hola [nombre]
 * Escribe un saludo, un cálculo y una línea con acentos (el terminal usa Latin-1; System.out
 * convierte desde UTF-8). Termina con el código de salida 0. */
public class Hola {
    public static void main(String[] args) {
        String quien = args.length > 0 ? args[0] : "mundo";
        System.out.println("hola " + quien + " desde Java en SaguiOS");
        System.out.println("2 + 3 = " + (2 + 3));
        System.out.println("raiz de 2 = " + Math.sqrt(2));
        System.out.println("acentos: año y canción, ¿qué tal?");
        System.exit(0);
    }
}
