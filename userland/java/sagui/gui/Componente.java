package sagui.gui;

/**
 * Componente - la base de todo lo que se ve dentro de una {@link Ventana}: tiene posición y tamaño
 * (en píxeles, relativos a la ventana), sabe dibujarse y recibe los eventos del ratón y el teclado
 * con coordenadas locales. Los componentes se dibujan con el tema de SaguiOS (clase {@link Colores}).
 */
public abstract class Componente {
    public int x, y, ancho, alto;
    public boolean visible = true;
    public boolean activo = true;
    Ventana ventana;

    public Componente posicion(int x, int y, int ancho, int alto) {
        this.x = x; this.y = y; this.ancho = ancho; this.alto = alto;
        repintar();
        return this;
    }

    /** Pide que la ventana se vuelva a dibujar. */
    public void repintar() {
        if (ventana != null) {
            ventana.repintar();
        }
    }

    public Ventana ventana() { return ventana; }

    /** Dibuja el componente; (0,0) es su esquina superior izquierda y todo se recorta a su área. */
    protected abstract void dibujar(Grafico g);

    /** Clic del ratón en (x, y) locales; devuelve true si lo trató. */
    protected boolean clic(int x, int y, int botones, int clics) { return false; }
    protected void soltar(int x, int y) { }
    protected void mover(int x, int y) { }
    protected void rueda(int muescas) { }
    /** Tecla (carácter Latin-1 o código de {@link Eventos}); devuelve true si la trató. */
    protected boolean tecla(int tecla, int modificadores) { return false; }
    /** ¿Puede recibir el foco del teclado? */
    protected boolean enfocable() { return false; }
    /** Se llama cada vez que cambia el foco. */
    protected void foco(boolean tiene) { repintar(); }

    final boolean contiene(int px, int py) { return visible && px >= x && py >= y && px < x + ancho && py < y + alto; }
    public final boolean tieneFoco() { return ventana != null && ventana.enFoco() == this; }
}
