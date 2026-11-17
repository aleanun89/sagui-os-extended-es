package sagui.gui;

import java.util.ArrayList;
import java.util.List;

/** Lista - una lista de textos con desplazamiento; un doble clic (o Intro) ejecuta su {@link Accion}. */
public class Lista extends Componente {
    private final List<String> elementos = new ArrayList<String>();
    private int seleccionado = -1;
    private int primero;
    private Accion alElegir;
    private Accion alCambiar;
    private static final int ALTO_ITEM = 20;

    public Lista(int x, int y, int ancho, int alto) { posicion(x, y, ancho, alto); }

    public void agregar(String t) { elementos.add(t); repintar(); }
    public void limpiar() { elementos.clear(); seleccionado = -1; primero = 0; repintar(); }
    public int cantidad() { return elementos.size(); }
    public String elemento(int i) { return elementos.get(i); }
    public int seleccionado() { return seleccionado; }
    public String textoSeleccionado() { return seleccionado < 0 ? null : elementos.get(seleccionado); }
    public void seleccionar(int i) { seleccionado = i; verSeleccionado(); repintar(); }
    /** Doble clic o Intro sobre un elemento. */
    public void alElegir(Accion a) { alElegir = a; }
    /** Cambia la selección. */
    public void alCambiar(Accion a) { alCambiar = a; }

    private int visibles() { return Math.max(1, (alto - 2) / ALTO_ITEM); }

    private void verSeleccionado() {
        if (seleccionado < primero) {
            primero = seleccionado;
        }
        if (seleccionado >= primero + visibles()) {
            primero = seleccionado - visibles() + 1;
        }
    }

    protected boolean enfocable() { return true; }

    protected void dibujar(Grafico g) {
        g.color(Colores.BLANCO);
        g.rectRelleno(0, 0, ancho, alto);
        for (int i = 0; i < visibles() && primero + i < elementos.size(); i++) {
            int k = primero + i;
            if (k == seleccionado) {
                g.color(tieneFoco() ? Colores.SELECCION : Colores.BORDE);
                g.rectRelleno(1, 1 + i * ALTO_ITEM, ancho - 2, ALTO_ITEM);
                g.color(Colores.BLANCO);
            } else {
                g.color(Colores.TEXTO);
            }
            g.texto(elementos.get(k), 6, 1 + i * ALTO_ITEM + 2);
        }
        g.color(tieneFoco() ? Colores.SELECCION : Colores.BORDE);
        g.rect(0, 0, ancho, alto);
    }

    private void cambiar(int nuevo) {
        if (nuevo < 0 || nuevo >= elementos.size() || nuevo == seleccionado) {
            return;
        }
        seleccionado = nuevo;
        verSeleccionado();
        repintar();
        if (alCambiar != null) {
            alCambiar.ejecutar(this);
        }
    }

    protected boolean clic(int px, int py, int botones, int clics) {
        int k = primero + (py - 1) / ALTO_ITEM;
        cambiar(k);
        if (clics >= 2 && seleccionado >= 0 && alElegir != null) {
            alElegir.ejecutar(this);
        }
        return true;
    }

    protected void rueda(int muescas) {
        primero = Math.max(0, Math.min(Math.max(0, elementos.size() - visibles()), primero + muescas * 3));
        repintar();
    }

    protected boolean tecla(int t, int mods) {
        if (t == Eventos.ABAJO) {
            cambiar(seleccionado < 0 ? 0 : seleccionado + 1);
        } else if (t == Eventos.ARRIBA) {
            cambiar(seleccionado < 0 ? 0 : seleccionado - 1);
        } else if (t == Eventos.INICIO) {
            cambiar(0);
        } else if (t == Eventos.FIN) {
            cambiar(elementos.size() - 1);
        } else if (t == '\n' && seleccionado >= 0 && alElegir != null) {
            alElegir.ejecutar(this);
        } else {
            return false;
        }
        return true;
    }
}
