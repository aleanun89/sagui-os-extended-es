import java.util.*;
import sagui.gui.*;

/**
 * Serpiente.java - un juego sencillo con el Lienzo (sagui.gui, D376): la serpiente come manzanas.
 *
 *   java -cp /apps/java/ejemplos Serpiente
 *
 * Flechas para girar, P pausa, R reinicia, Q sale. Un temporizador de la ventana mueve la serpiente.
 * Muestra Lienzo.pintar, el teclado y Ventana.cadaMilisegundos.
 */
public class Serpiente extends Lienzo {
    static final int N = 20, CELDA = 16;
    LinkedList<int[]> cuerpo = new LinkedList<int[]>();
    int dx = 1, dy = 0, manzanaX, manzanaY, puntos;
    boolean fin, pausa;
    Random azar = new Random(7);
    static Ventana ventana;

    Serpiente() {
        super(0, 0, N * CELDA, N * CELDA + 24);
        reiniciar();
    }

    void reiniciar() {
        cuerpo.clear();
        cuerpo.add(new int[] { 10, 10 });
        cuerpo.add(new int[] { 9, 10 });
        cuerpo.add(new int[] { 8, 10 });
        dx = 1; dy = 0; puntos = 0; fin = false; pausa = false;
        manzana();
    }

    void manzana() {
        do {
            manzanaX = azar.nextInt(N);
            manzanaY = azar.nextInt(N);
        } while (enCuerpo(manzanaX, manzanaY));
    }

    boolean enCuerpo(int x, int y) {
        for (int[] c : cuerpo) {
            if (c[0] == x && c[1] == y) return true;
        }
        return false;
    }

    void paso() {
        if (fin || pausa) return;
        int[] cab = cuerpo.getFirst();
        int nx = cab[0] + dx, ny = cab[1] + dy;
        if (nx < 0 || ny < 0 || nx >= N || ny >= N || enCuerpo(nx, ny)) {
            fin = true;
            return;
        }
        cuerpo.addFirst(new int[] { nx, ny });
        if (nx == manzanaX && ny == manzanaY) {
            puntos++;
            manzana();
        } else {
            cuerpo.removeLast();
        }
    }

    protected void pintar(Grafico g) {
        g.color(Colores.rgb(30, 30, 40));
        g.rectRelleno(0, 0, N * CELDA, N * CELDA);
        g.color(Colores.ROJO);
        g.elipseRellena(manzanaX * CELDA + 1, manzanaY * CELDA + 1, CELDA - 2, CELDA - 2);
        boolean primero = true;
        for (int[] c : cuerpo) {
            g.color(primero ? Colores.AMARILLO : Colores.VERDE);
            g.rectRelleno(c[0] * CELDA + 1, c[1] * CELDA + 1, CELDA - 2, CELDA - 2);
            primero = false;
        }
        g.color(Colores.TEXTO);
        g.texto("Puntos: " + puntos + (fin ? "   FIN (R reinicia)" : pausa ? "   PAUSA" : ""), 4, N * CELDA + 4);
    }

    protected void teclaPulsada(int t, int mods) {
        if (t == Eventos.ARRIBA && dy == 0) { dx = 0; dy = -1; }
        else if (t == Eventos.ABAJO && dy == 0) { dx = 0; dy = 1; }
        else if (t == Eventos.IZQUIERDA && dx == 0) { dx = -1; dy = 0; }
        else if (t == Eventos.DERECHA && dx == 0) { dx = 1; dy = 0; }
        else if (t == 'p' || t == 'P') pausa = !pausa;
        else if (t == 'r' || t == 'R') reiniciar();
        else if (t == 'q' || t == 'Q') ventana.cerrar();
        repintar();
    }

    public static void main(String[] args) {
        final Serpiente juego = new Serpiente();
        Ventana v = new Ventana("Serpiente", N * CELDA, N * CELDA + 24);
        ventana = v;
        v.agregar(juego);
        v.enfocar(juego);
        v.cadaMilisegundos(120, new Runnable() {
            public void run() { juego.paso(); }
        });
        v.ejecutar();
        System.out.println("serpiente: puntos " + juego.puntos);
    }
}
