package sagui.gui;

import java.io.IOException;

/** Imagen - un fichero PNG, JPEG, BMP o GIF cargado en memoria. */
public class Imagen {
    final int id;
    public final int ancho, alto;

    private Imagen(int id) {
        this.id = id;
        this.ancho = Nativo.imagenAncho(id);
        this.alto = Nativo.imagenAlto(id);
    }

    public static Imagen cargar(String ruta) throws IOException {
        int h = Nativo.cargarImagen(ruta);
        if (h <= 0) {
            throw new IOException("no se pudo leer la imagen " + ruta + " (" + h + ")");
        }
        return new Imagen(h);
    }

    public void liberar() { Nativo.liberarImagen(id); }
}
