package sagui.gui;

/**
 * Grafico - el contexto de dibujo 2D de un {@link Lienzo}: línea, rectángulo, elipse, texto e imágenes.
 * Las coordenadas son relativas al lienzo y todo se recorta a su área.
 */
public class Grafico {
    private final int v;
    private final int ox, oy;
    private int color = Colores.NEGRO;
    private Fuente fuente = Fuente.SISTEMA;

    Grafico(int ventana, int ox, int oy) { this.v = ventana; this.ox = ox; this.oy = oy; }

    public void color(int rgb) { color = rgb; }
    public void fuente(Fuente f) { fuente = f; }
    public Fuente fuente() { return fuente; }

    public void linea(int x0, int y0, int x1, int y1) { Nativo.linea(v, ox + x0, oy + y0, ox + x1, oy + y1, color); }
    public void rect(int x, int y, int ancho, int alto) {
        linea(x, y, x + ancho - 1, y);
        linea(x, y + alto - 1, x + ancho - 1, y + alto - 1);
        linea(x, y, x, y + alto - 1);
        linea(x + ancho - 1, y, x + ancho - 1, y + alto - 1);
    }
    public void rectRelleno(int x, int y, int ancho, int alto) { Nativo.rect(v, ox + x, oy + y, ancho, alto, color); }
    public void elipse(int x, int y, int ancho, int alto) { Nativo.elipse(v, ox + x, oy + y, ancho, alto, color, false); }
    public void elipseRellena(int x, int y, int ancho, int alto) { Nativo.elipse(v, ox + x, oy + y, ancho, alto, color, true); }
    public void punto(int x, int y) { Nativo.rect(v, ox + x, oy + y, 1, 1, color); }
    public void texto(String t, int x, int y) { Nativo.texto(v, ox + x, oy + y, t, color, fuente.id, fuente.px); }
    public int anchoTexto(String t) { return fuente.ancho(t); }
    public void imagen(Imagen im, int x, int y) { Nativo.dibujarImagen(v, im.id, ox + x, oy + y, im.ancho, im.alto); }
    public void imagen(Imagen im, int x, int y, int ancho, int alto) { Nativo.dibujarImagen(v, im.id, ox + x, oy + y, ancho, alto); }
}
