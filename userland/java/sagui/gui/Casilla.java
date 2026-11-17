package sagui.gui;

/** Casilla - una casilla de verificación con texto. */
public class Casilla extends Componente {
    private String texto;
    private boolean marcada;
    private Accion accion;

    public Casilla(String texto, boolean marcada, Accion accion) {
        this.texto = texto;
        this.marcada = marcada;
        this.accion = accion;
        this.alto = 20;
        this.ancho = 24 + Fuente.SISTEMA.ancho(texto);
    }

    public boolean marcada() { return marcada; }
    public void marcada(boolean m) { marcada = m; repintar(); }

    protected boolean enfocable() { return true; }

    protected void dibujar(Grafico g) {
        g.color(Colores.BLANCO);
        g.rectRelleno(2, 3, 14, 14);
        g.color(tieneFoco() ? Colores.DESTACADO : Colores.BORDE);
        g.rect(2, 3, 14, 14);
        if (marcada) {
            g.color(Colores.DESTACADO);
            g.linea(5, 10, 8, 14);
            g.linea(8, 14, 14, 6);
            g.linea(5, 11, 8, 15);
            g.linea(8, 15, 14, 7);
        }
        g.color(activo ? Colores.TEXTO : Colores.TEXTO_APAGADO);
        g.texto(texto, 22, 2);
    }

    private void alternar() {
        marcada = !marcada;
        repintar();
        if (accion != null) {
            accion.ejecutar(this);
        }
    }

    protected boolean clic(int px, int py, int botones, int clics) {
        if (activo) {
            alternar();
        }
        return true;
    }

    protected boolean tecla(int t, int mods) {
        if (activo && t == ' ') {
            alternar();
            return true;
        }
        return false;
    }
}
