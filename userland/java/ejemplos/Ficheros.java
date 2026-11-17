import java.io.*;

/* Ficheros.java - escribir, leer y borrar ficheros con java.io (en /tmp).
 *
 *   java -cp /apps/java/ejemplos Ficheros */
public class Ficheros {
    public static void main(String[] args) throws IOException {
        File f = new File("/tmp/java_ficheros.txt");
        PrintWriter w = new PrintWriter(new FileWriter(f));
        for (int i = 1; i <= 3; i++) {
            w.println("linea " + i);
        }
        w.close();
        System.out.println("escrito: " + f.length() + " bytes");

        BufferedReader r = new BufferedReader(new FileReader(f));
        String l;
        int n = 0;
        while ((l = r.readLine()) != null) {
            n++;
            System.out.println("leido: " + l);
        }
        r.close();
        System.out.println("lineas: " + n);
        System.out.println("existe: " + f.exists());
        f.delete();
        System.out.println("existe tras borrar: " + f.exists());
        System.out.println("ficheros: terminado");
    }
}
