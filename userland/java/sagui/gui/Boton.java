package sagui.gui;

/** Boton - un botón con texto; al soltarlo dentro de su área ejecuta su {@link Accion}. */
public class Boton extends Componente {
    private String texto;
    private Accion accion;
    private boolean pulsado;
    public Fuente fuente = Fuente.SISTEMA;

    public Boton(String texto, Accion accion) { this.texto = texto; this.accion = accion; }

    public Boton(String texto, int x, int y, int ancho, int alto, Accion accion) {
        this(texto, accion);
        posicion(x, y, ancho, alto);
    }

    public String texto() { return texto; }
    public void texto(String t) { texto = t; repintar(); }
    public void alPulsar(Accion a) { accion = a; }

    protected boolean enfocable() { return true; }

    protected void dibujar(Grafico g) {
        g.color(pulsado ? Colores.BOTON_PULSADO : Colores.BOTON);
        g.rectRelleno(0, 0, ancho, alto);
        g.color(tieneFoco() ? Colores.DESTACADO : Colores.BORDE);
        g.rect(0, 0, ancho, alto);
        g.color(activo ? Colores.TEXTO : Colores.TEXTO_APAGADO);
        g.fuente(fuente);
        int tx = (ancho - fuente.ancho(texto)) / 2, ty = (alto - fuente.alto()) / 2;
        g.texto(texto, tx + (pulsado ? 1 : 0), ty + (pulsado ? 1 : 0));
    }

    protected boolean clic(int px, int py, int botones, int clics) {
        if (!activo) {
            return true;
        }
        pulsado = true;
        repintar();
        return true;
    }

    protected void soltar(int px, int py) {
        boolean dentro = px >= 0 && py >= 0 && px < ancho && py < alto;
        boolean era = pulsado;
        pulsado = false;
        repintar();
        if (era && dentro && activo && accion != null) {
            accion.ejecutar(this);
        }
    }

    protected boolean tecla(int t, int mods) {
        if (activo && (t == '\n' || t == ' ') && accion != null) {
            accion.ejecutar(this);
            return true;
        }
        return false;
    }
}
