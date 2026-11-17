package sagui.gui;

/** Etiqueta - un texto fijo (varias líneas con '\n'). */
public class Etiqueta extends Componente {
    private String texto;
    public int color = Colores.TEXTO;
    public Fuente fuente = Fuente.SISTEMA;

    public Etiqueta(String texto) { this.texto = texto; }

    public Etiqueta(String texto, int x, int y) {
        this(texto);
        posicion(x, y, 0, 0);
        ajustar();
    }

    public String texto() { return texto; }
    public void texto(String t) { texto = t; ajustar(); repintar(); }

    /** Ajusta el tamaño al texto. */
    public void ajustar() {
        String[] l = texto.split("\n");
        int w = 0;
        for (String s : l) {
            w = Math.max(w, fuente.ancho(s));
        }
        ancho = w;
        alto = l.length * fuente.alto();
    }

    protected void dibujar(Grafico g) {
        g.color(color);
        g.fuente(fuente);
        String[] l = texto.split("\n");
        for (int i = 0; i < l.length; i++) {
            g.texto(l[i], 0, i * fuente.alto());
        }
    }
}
