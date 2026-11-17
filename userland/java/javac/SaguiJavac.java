import java.io.File;
import java.util.ArrayList;
import java.util.List;
import org.codehaus.janino.Compiler;

/* SaguiJavac.java - la orden «javac» de SaguiOS (D376): un envoltorio de línea de órdenes sobre el
 * compilador Janino (BSD-3), que corre sobre miniJVM.
 *
 *   javac [-d directorio] [-cp ruta:ruta] Fuente.java...
 *
 * Janino entiende casi todo Java 8 salvo algunas construcciones de genéricos avanzados y lambdas
 * complejas; para programas grandes se compila en el PC con tools/javac-sagui. */
public class SaguiJavac {
    public static void main(String[] args) throws Exception {
        File destino = new File(".");
        List<File> fuentes = new ArrayList<File>();
        List<File> cp = new ArrayList<File>();
        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("-d") && i + 1 < args.length) {
                destino = new File(args[++i]);
            } else if ((args[i].equals("-cp") || args[i].equals("-classpath")) && i + 1 < args.length) {
                for (String p : args[++i].split(":")) {
                    cp.add(new File(p));
                }
            } else if (args[i].endsWith(".java")) {
                fuentes.add(new File(args[i]));
            } else {
                System.out.println("javac: opción ignorada: " + args[i]);
            }
        }
        if (fuentes.isEmpty()) {
            System.out.println("Uso: javac [-d directorio] [-cp ruta:ruta] Fuente.java...");
            System.exit(2);
        }
        destino.mkdirs();
        Compiler c = new Compiler();
        c.setBootClassPath(new File[] { new File("/apps/java/lib/minijvm_rt.jar") });
        cp.add(new File("/apps/java/lib/sagui.jar"));
        cp.add(new File("."));
        c.setClassPath(cp.toArray(new File[0]));
        c.setDestinationDirectory(destino, false);
        try {
            if (!c.compile(fuentes.toArray(new File[0]))) {
                System.exit(1);
            }
        } catch (Exception e) {
            System.out.println("javac: " + e);
            System.exit(1);
        }
    }
}
