package sagui.gui;

import java.util.ArrayList;
import java.util.List;

/**
 * Fuente - una fuente de texto. {@link #SISTEMA} es la de mapa de bits del escritorio (8x16);
 * {@link #cargar(String)} abre una TrueType/OpenType (por ejemplo de /apps/fuentes) con un tamaño en píxeles.
 */
public class Fuente {
    public static final Fuente SISTEMA = new Fuente(0, 16);
    final int id;
    final int px;

    private Fuente(int id, int px) { this.id = id; this.px = px; }

    /** Abre una fuente vectorial; devuelve null si no existe. */
    public static Fuente cargar(String ruta, int tamano) {
        int h = Nativo.abrirFuente(ruta);
        return h == 0 ? null : new Fuente(h, tamano);
    }

    /** La misma fuente con otro tamaño en píxeles. */
    public Fuente conTamano(int tamano) { return new Fuente(id, tamano); }

    public int ancho(String t) { return Nativo.anchoTexto(t, id, px); }
    public int alto() { return id == 0 ? 16 : px + px / 4; }

    /** Los ficheros de fuentes del sistema (por defecto /apps/fuentes). */
    public static String[] delSistema() {
        return delSistema("/apps/fuentes");
    }

    public static String[] delSistema(String carpeta) {
        String s = Nativo.listarFuentes(carpeta);
        List<String> l = new ArrayList<String>();
        for (String n : s.split("\n")) {
            if (n.length() > 0) {
                l.add(carpeta + "/" + n);
            }
        }
        return l.toArray(new String[0]);
    }
}
