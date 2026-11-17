package sagui.gui;

/**
 * Lienzo - una superficie de dibujo 2D. Se hereda y se redefine {@link #pintar(Grafico)}; el ratón y
 * el teclado llegan a {@link #ratonPulsado}, {@link #ratonMovido}, {@link #ratonSoltado} y
 * {@link #teclaPulsada}. Para animar, se pide repintar con {@link #repintar()} desde un temporizador.
 */
public class Lienzo extends Componente {
    public int fondo = Colores.BLANCO;

    public Lienzo(int x, int y, int ancho, int alto) { posicion(x, y, ancho, alto); }

    /** Dibuja el contenido; el lienzo ya está borrado con el color de fondo. */
    protected void pintar(Grafico g) { }
    protected void ratonPulsado(int x, int y, int boton) { }
    protected void ratonSoltado(int x, int y) { }
    protected void ratonMovido(int x, int y) { }
    protected void teclaPulsada(int tecla, int modificadores) { }

    protected boolean enfocable() { return true; }

    protected final void dibujar(Grafico g) {
        g.color(fondo);
        g.rectRelleno(0, 0, ancho, alto);
        g.color(Colores.NEGRO);
        pintar(g);
    }

    protected boolean clic(int px, int py, int botones, int clics) { ratonPulsado(px, py, botones); return true; }
    protected void soltar(int px, int py) { ratonSoltado(px, py); }
    protected void mover(int px, int py) { ratonMovido(px, py); }
    protected boolean tecla(int t, int mods) { teclaPulsada(t, mods); return true; }
}
