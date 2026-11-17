import java.io.*;

/* Adivinar.java - un pequeño juego de consola: adivina el número entre 1 y 100.
 *
 *   java -cp /apps/java/ejemplos Adivinar [numero_secreto]
 * Lee de la entrada estándar (sirve una tubería: echo 50 | java ... Adivinar 42). */
public class Adivinar {
    public static void main(String[] args) throws IOException {
        int secreto = args.length > 0 ? Integer.parseInt(args[0])
                                      : 1 + (int) (System.currentTimeMillis() % 100);
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        System.out.println("Adivina el numero (1-100):");
        for (int intento = 1; intento <= 10; intento++) {
            String l = in.readLine();
            if (l == null) {
                System.out.println("sin mas entrada");
                return;
            }
            int n;
            try {
                n = Integer.parseInt(l.trim());
            } catch (NumberFormatException e) {
                System.out.println("no es un numero");
                continue;
            }
            if (n < secreto) {
                System.out.println("mas alto");
            } else if (n > secreto) {
                System.out.println("mas bajo");
            } else {
                System.out.println("acertaste en " + intento + " intentos");
                return;
            }
        }
        System.out.println("perdiste, era " + secreto);
    }
}
