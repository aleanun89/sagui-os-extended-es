package sagui.gui;

import java.util.ArrayList;
import java.util.List;

/**
 * Menu - un menú desplegable de la {@link BarraMenu}: una lista de elementos con su {@link Accion}.
 * Un elemento cuyo texto es "-" es un separador.
 */
public class Menu {
    final String titulo;
    final List<String> textos = new ArrayList<String>();
    final List<Accion> acciones = new ArrayList<Accion>();

    public Menu(String titulo) { this.titulo = titulo; }

    public Menu elemento(String texto, Accion accion) {
        textos.add(texto);
        acciones.add(accion);
        return this;
    }

    public Menu separador() { return elemento("-", null); }
}
