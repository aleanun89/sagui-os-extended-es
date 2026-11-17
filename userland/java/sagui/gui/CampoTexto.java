package sagui.gui;

/**
 * CampoTexto - una línea de texto editable: cursor, Inicio/Fin, Retroceso, Suprimir, pegar con Ctrl+V.
 * Con Intro ejecuta su {@link Accion}.
 */
public class CampoTexto extends Componente {
    private StringBuilder texto = new StringBuilder();
    private int cursor;
    private int desplazamiento;
    private Accion accion;
    public int maximo = 256;

    public CampoTexto(String inicial, int x, int y, int ancho, Accion alIntro) {
        texto.append(inicial);
        cursor = texto.length();
        this.accion = alIntro;
        posicion(x, y, ancho, 24);
    }

    public String texto() { return texto.toString(); }
    public void texto(String t) { texto = new StringBuilder(t); cursor = texto.length(); repintar(); }

    protected boolean enfocable() { return true; }

    protected void dibujar(Grafico g) {
        g.color(Colores.BLANCO);
        g.rectRelleno(0, 0, ancho, alto);
        g.color(tieneFoco() ? Colores.SELECCION : Colores.BORDE);
        g.rect(0, 0, ancho, alto);
        // el trozo visible: desde desplazamiento hasta que se llena
        while (cursor < desplazamiento) {
            desplazamiento--;
        }
        while (Fuente.SISTEMA.ancho(texto.substring(desplazamiento, cursor)) > ancho - 10) {
            desplazamiento++;
        }
        g.color(Colores.TEXTO);
        String vis = texto.substring(desplazamiento);
        int n = vis.length();
        while (n > 0 && Fuente.SISTEMA.ancho(vis.substring(0, n)) > ancho - 8) {
            n--;
        }
        g.texto(vis.substring(0, n), 4, (alto - 16) / 2);
        if (tieneFoco()) {
            int cx = 4 + Fuente.SISTEMA.ancho(texto.substring(desplazamiento, cursor));
            g.color(Colores.TEXTO);
            g.linea(cx, 3, cx, alto - 4);
        }
    }

    protected boolean clic(int px, int py, int botones, int clics) {
        int i = desplazamiento;
        while (i < texto.length() && 4 + Fuente.SISTEMA.ancho(texto.substring(desplazamiento, i + 1)) <= px) {
            i++;
        }
        cursor = i;
        repintar();
        return true;
    }

    protected boolean tecla(int t, int mods) {
        if (!activo) {
            return false;
        }
        if (t == '\n') {
            if (accion != null) {
                accion.ejecutar(this);
            }
        } else if (t == '\b') {
            if (cursor > 0) {
                texto.deleteCharAt(--cursor);
            }
        } else if (t == Eventos.SUPRIMIR) {
            if (cursor < texto.length()) {
                texto.deleteCharAt(cursor);
            }
        } else if (t == Eventos.IZQUIERDA) {
            if (cursor > 0) {
                cursor--;
            }
        } else if (t == Eventos.DERECHA) {
            if (cursor < texto.length()) {
                cursor++;
            }
        } else if (t == Eventos.INICIO) {
            cursor = 0;
        } else if (t == Eventos.FIN) {
            cursor = texto.length();
        } else if ((mods & Eventos.MOD_CTRL) != 0 && (t == 'v' || t == 'V' || t == 22)) {
            String p = Nativo.pegar();
            if (p.length() > 0 && texto.length() + p.length() <= maximo) {
                texto.insert(cursor, p);
                cursor += p.length();
            }
        } else if (t >= 0x20 && t < 0x100 && t != 0x7F && texto.length() < maximo) {
            texto.insert(cursor++, (char) t);
        } else {
            return false;
        }
        repintar();
        return true;
    }
}
