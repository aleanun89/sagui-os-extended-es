import java.io.*;

/* Acentos.java - texto fuera de ASCII: System.out convierte UTF-8 a Latin-1 para el terminal (D376).
 *
 *   java -cp /apps/java/ejemplos Acentos
 * Escribe «año y canción», pide una línea (System.in entrega Latin-1, que Java lee tal cual: la ñ es un
 * carácter), la mide y guarda un fichero en UTF-8. El euro no cabe en Latin-1: sale como «?». */
public class Acentos {
    public static void main(String[] args) throws IOException {
        System.out.println("año y canción");
        System.out.println("escribe algo:");
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        String l = in.readLine();
        System.out.println("leido: " + l + " (" + l.length() + " caracteres, "
                           + l.getBytes("UTF-8").length + " bytes UTF-8)");
        FileOutputStream f = new FileOutputStream("/tmp/java_acentos.txt");
        f.write("año y canción".getBytes("UTF-8"));
        f.close();
        byte[] b = "año".getBytes("UTF-8");
        System.out.println("año en UTF-8: " + (b[0] & 255) + " " + (b[1] & 255) + " " + (b[2] & 255) + " " + (b[3] & 255) + " (" + b.length + " bytes)");
        FileInputStream r = new FileInputStream("/tmp/java_acentos.txt");
        int n = 0;
        while (r.read() >= 0) {
            n++;
        }
        r.close();
        System.out.println("bytes en el fichero: " + n);
        System.out.println("euro: € no cabe");
    }
}
