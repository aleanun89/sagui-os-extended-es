package sagui.gui;

import java.util.ArrayList;
import java.util.List;

/**
 * BarraMenu - la barra de menús de una ventana (arriba del todo, 22 píxeles). Se añade a la
 * ventana con {@link Ventana#barraMenu(BarraMenu)}. Los menús se despliegan con el ratón.
 */
public class BarraMenu extends Componente {
    private final List<Menu> menus = new ArrayList<Menu>();
    private int abierto = -1;
    private int resaltado = -1;
    static final int ALTO = 22, ALTO_ITEM = 20;

    public BarraMenu agregar(Menu m) { menus.add(m); return this; }

    private int inicioMenu(int i) {
        int x = 4;
        for (int k = 0; k < i; k++) {
            x += Fuente.SISTEMA.ancho(menus.get(k).titulo) + 16;
        }
        return x;
    }

    private int anchoDesplegable(Menu m) {
        int w = 60;
        for (String t : m.textos) {
            w = Math.max(w, Fuente.SISTEMA.ancho(t) + 24);
        }
        return w;
    }

    boolean estaAbierto() { return abierto >= 0; }
    void cerrar() { abierto = -1; resaltado = -1; repintar(); }

    protected void dibujar(Grafico g) {
        g.color(Colores.BOTON);
        g.rectRelleno(0, 0, ancho, ALTO);
        g.color(Colores.BORDE);
        g.linea(0, ALTO - 1, ancho, ALTO - 1);
        for (int i = 0; i < menus.size(); i++) {
            int x = inicioMenu(i), w = Fuente.SISTEMA.ancho(menus.get(i).titulo) + 16;
            if (i == abierto) {
                g.color(Colores.SELECCION);
                g.rectRelleno(x - 4, 0, w, ALTO - 1);
                g.color(Colores.BLANCO);
            } else {
                g.color(Colores.TEXTO);
            }
            g.texto(menus.get(i).titulo, x + 4, 3);
        }
    }

    /** Dibuja el menú desplegado (lo llama la ventana al final, encima de todo). */
    void dibujarDesplegable(Grafico g) {
        if (abierto < 0) {
            return;
        }
        Menu m = menus.get(abierto);
        int x = inicioMenu(abierto) - 4, w = anchoDesplegable(m), h = m.textos.size() * ALTO_ITEM + 4;
        g.color(Colores.BLANCO);
        g.rectRelleno(x, ALTO - 1, w, h);
        g.color(Colores.BORDE);
        g.rect(x, ALTO - 1, w, h);
        for (int i = 0; i < m.textos.size(); i++) {
            int y = ALTO + 1 + i * ALTO_ITEM;
            String t = m.textos.get(i);
            if (t.equals("-")) {
                g.color(Colores.BORDE);
                g.linea(x + 4, y + ALTO_ITEM / 2, x + w - 5, y + ALTO_ITEM / 2);
                continue;
            }
            if (i == resaltado) {
                g.color(Colores.SELECCION);
                g.rectRelleno(x + 2, y, w - 4, ALTO_ITEM);
                g.color(Colores.BLANCO);
            } else {
                g.color(Colores.TEXTO);
            }
            g.texto(t, x + 12, y + 2);
        }
    }

    /** Clic en coordenadas de la ventana; devuelve true si lo consumió. */
    boolean clicVentana(int px, int py) {
        if (py >= 0 && py < ALTO) {
            for (int i = 0; i < menus.size(); i++) {
                int x = inicioMenu(i);
                if (px >= x - 4 && px < x + Fuente.SISTEMA.ancho(menus.get(i).titulo) + 12) {
                    abierto = abierto == i ? -1 : i;
                    resaltado = -1;
                    repintar();
                    return true;
                }
            }
            cerrar();
            return true;
        }
        if (abierto < 0) {
            return false;
        }
        Menu m = menus.get(abierto);
        int x = inicioMenu(abierto) - 4, w = anchoDesplegable(m);
        int k = (py - ALTO - 1) / ALTO_ITEM;
        if (px >= x && px < x + w && py > ALTO && k >= 0 && k < m.textos.size()) {
            Accion a = m.acciones.get(k);
            cerrar();
            if (a != null) {
                a.ejecutar(this);
            }
        } else {
            cerrar();
        }
        return true;
    }

    void moverVentana(int px, int py) {
        if (abierto < 0) {
            return;
        }
        Menu m = menus.get(abierto);
        int x = inicioMenu(abierto) - 4, w = anchoDesplegable(m);
        int k = (py - ALTO - 1) / ALTO_ITEM;
        int nuevo = (px >= x && px < x + w && py > ALTO && k < m.textos.size()) ? k : -1;
        if (nuevo != resaltado) {
            resaltado = nuevo;
            repintar();
        }
    }
}
