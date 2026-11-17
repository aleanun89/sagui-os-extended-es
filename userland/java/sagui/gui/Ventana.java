package sagui.gui;

import java.util.ArrayList;
import java.util.List;

/**
 * Ventana - una ventana de SaguiOS con sus componentes. Se crean los componentes, se añaden con
 * {@link #agregar(Componente)} y se llama a {@link #ejecutar()}, que dibuja y reparte los eventos
 * (ratón, teclado, temporizadores) hasta que se cierra la ventana.
 *
 * <pre>
 * Ventana v = new Ventana("Hola", 300, 120);
 * v.agregar(new Boton("Salir", 100, 40, 80, 28, c -&gt; v.cerrar()));
 * v.ejecutar();
 * </pre>
 *
 * Todo el código de la interfaz corre en un solo hilo (el que llama a ejecutar); desde otros hilos
 * solo se puede llamar a {@link #repintar()}.
 */
public class Ventana {
    private final int id;
    private final List<Componente> hijos = new ArrayList<Componente>();
    private Componente foco;
    private Componente agarrado;
    private BarraMenu barra;
    private boolean sucio = true;
    private boolean cerrada;
    private int ancho, alto;
    private Accion alCerrar;
    private Accion alRedimensionar;
    public int fondo = Colores.FONDO;

    private static class Temporizador {
        Runnable tarea;
        int periodo;
        int proximo;
        boolean repetir = true;
    }
    private final List<Temporizador> temporizadores = new ArrayList<Temporizador>();

    public Ventana(String titulo, int ancho, int alto) {
        id = Nativo.abrir(titulo, ancho, alto);
        if (id == 0) {
            throw new IllegalStateException("no se pudo abrir la ventana (hay pantalla grafica?)");
        }
        this.ancho = Nativo.ancho(id);
        this.alto = Nativo.alto(id);
    }

    public int ancho() { return ancho; }
    public int alto() { return alto; }
    public void titulo(String t) { Nativo.titulo(id, t); }

    public Ventana agregar(Componente c) {
        c.ventana = this;
        hijos.add(c);
        if (foco == null && c.enfocable()) {
            enfocar(c);
        }
        sucio = true;
        return this;
    }

    public void quitar(Componente c) {
        hijos.remove(c);
        if (foco == c) {
            foco = null;
        }
        c.ventana = null;
        sucio = true;
    }

    public void barraMenu(BarraMenu b) {
        barra = b;
        b.ventana = this;
        b.posicion(0, 0, ancho, BarraMenu.ALTO);
    }

    public Componente enFoco() { return foco; }

    public void enfocar(Componente c) {
        if (foco == c) {
            return;
        }
        Componente anterior = foco;
        foco = c;
        if (anterior != null) {
            anterior.foco(false);
        }
        if (c != null) {
            c.foco(true);
        }
    }

    /** Pide que se vuelva a dibujar la ventana. Se puede llamar desde cualquier hilo. */
    public void repintar() { sucio = true; }

    /** Ejecuta la tarea cada periodo milisegundos (en el hilo de la interfaz). */
    public void cadaMilisegundos(int periodo, Runnable tarea) {
        Temporizador t = new Temporizador();
        t.tarea = tarea;
        t.periodo = periodo;
        t.proximo = Nativo.milisegundos() + periodo;
        temporizadores.add(t);
    }

    /** Qué hacer cuando cierran la ventana con la X (por defecto, terminar {@link #ejecutar()}). */
    public void alCerrar(Accion a) { alCerrar = a; }
    public void alRedimensionar(Accion a) { alRedimensionar = a; }

    /** Termina {@link #ejecutar()} y cierra la ventana. */
    public void cerrar() { cerrada = true; }

    private void pintarTodo() {
        Nativo.recorte(id, 0, 0, 0, 0);
        Nativo.rect(id, 0, 0, ancho, alto, fondo);
        for (Componente c : hijos) {
            if (!c.visible) {
                continue;
            }
            Nativo.recorte(id, c.x, c.y, c.ancho, c.alto);
            c.dibujar(new Grafico(id, c.x, c.y));
        }
        Nativo.recorte(id, 0, 0, 0, 0);
        if (barra != null) {
            barra.dibujar(new Grafico(id, 0, 0));
            barra.dibujarDesplegable(new Grafico(id, 0, 0));
        }
        Nativo.mostrar(id);
        sucio = false;
    }

    private Componente bajoRaton(int px, int py) {
        for (int i = hijos.size() - 1; i >= 0; i--) {
            Componente c = hijos.get(i);
            if (c.contiene(px, py)) {
                return c;
            }
        }
        return null;
    }

    private void siguienteFoco(boolean atras) {
        int n = hijos.size();
        int i = hijos.indexOf(foco);
        for (int k = 1; k <= n; k++) {
            Componente c = hijos.get(((i + (atras ? -k : k)) % n + n) % n);
            if (c.enfocable() && c.visible && c.activo) {
                enfocar(c);
                return;
            }
        }
    }

    private void tratar(int[] e) {
        int tipo = e[0], px = e[1], py = e[2];
        if (tipo == Eventos.CERRAR) {
            if (alCerrar != null) {
                alCerrar.ejecutar(null);
            } else {
                cerrada = true;
            }
        } else if (tipo == Eventos.CLIC) {
            if (barra != null && (barra.estaAbierto() || py < BarraMenu.ALTO) && barra.clicVentana(px, py)) {
                return;
            }
            Componente c = bajoRaton(px, py);
            agarrado = c;
            if (c != null) {
                if (c.enfocable() && c.activo) {
                    enfocar(c);
                }
                c.clic(px - c.x, py - c.y, e[3], e[4]);
            }
            sucio = true;
        } else if (tipo == Eventos.SOLTAR) {
            if (agarrado != null) {
                agarrado.soltar(px - agarrado.x, py - agarrado.y);
                agarrado = null;
            }
        } else if (tipo == Eventos.MOVER) {
            if (barra != null) {
                barra.moverVentana(px, py);
            }
            Componente c = agarrado != null ? agarrado : bajoRaton(px, py);
            if (c != null) {
                c.mover(px - c.x, py - c.y);
            }
        } else if (tipo == Eventos.RUEDA) {
            Componente c = bajoRaton(px, py);
            if (c != null) {
                c.rueda((int) e[4]);
            }
        } else if (tipo == Eventos.TECLA) {
            int t = e[5], mods = e[6];
            if (barra != null && barra.estaAbierto() && t == Eventos.ESC) {
                barra.cerrar();
            } else if (foco != null && foco.tecla(t, mods)) {
                sucio = true;
            } else if (t == '\t') {
                siguienteFoco((mods & Eventos.MOD_MAYUS) != 0);
            }
            sucio = true;
        } else if (tipo == Eventos.REDIMENSIONAR) {
            ancho = Nativo.ancho(id);
            alto = Nativo.alto(id);
            if (barra != null) {
                barra.posicion(0, 0, ancho, BarraMenu.ALTO);
            }
            if (alRedimensionar != null) {
                alRedimensionar.ejecutar(null);
            }
            sucio = true;
        }
    }

    /** El bucle de eventos: dibuja, espera eventos, dispara temporizadores; vuelve al cerrarse la ventana. */
    public void ejecutar() {
        int[] e = new int[7];
        while (!cerrada) {
            if (sucio) {
                pintarTodo();
            }
            int espera = 100;
            int ahora = Nativo.milisegundos();
            for (Temporizador t : temporizadores) {
                espera = Math.max(1, Math.min(espera, t.proximo - ahora));
            }
            if (sucio) {
                espera = 1;
            }
            if (Nativo.evento(id, espera, e) == 1) {
                tratar(e);
            }
            ahora = Nativo.milisegundos();
            for (int i = 0; i < temporizadores.size(); i++) {
                Temporizador t = temporizadores.get(i);
                if (ahora - t.proximo >= 0) {
                    t.proximo = ahora + t.periodo;
                    t.tarea.run();
                    sucio = true;
                }
            }
        }
        Nativo.cerrar(id);
    }
}
