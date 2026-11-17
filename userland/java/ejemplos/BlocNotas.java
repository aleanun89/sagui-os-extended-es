import java.io.*;
import sagui.gui.*;

/**
 * BlocNotas.java - un bloc de notas sencillo con ventana (sagui.gui, D376): menú Archivo
 * (Nuevo, Abrir, Guardar, Salir) con los diálogos del sistema, y un área de texto escrita sobre Lienzo.
 *
 *   java -cp /apps/java/ejemplos BlocNotas [fichero]
 *
 * El área de texto es un Lienzo que guarda las líneas y el cursor y pinta con la fuente del sistema;
 * Retroceso, Intro, flechas y caracteres normales.
 */
public class BlocNotas {
    static class Editor extends Lienzo {
        java.util.List<StringBuilder> lineas = new java.util.ArrayList<StringBuilder>();
        int fila, col, primera;

        Editor(int x, int y, int w, int h) {
            super(x, y, w, h);
            nuevo();
        }

        void nuevo() {
            lineas.clear();
            lineas.add(new StringBuilder());
            fila = col = primera = 0;
            repintar();
        }

        String texto() {
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < lineas.size(); i++) {
                sb.append(lineas.get(i)).append('\n');
            }
            return sb.toString();
        }

        void cargar(String t) {
            nuevo();
            lineas.clear();
            for (String l : t.split("\n", -1)) {
                lineas.add(new StringBuilder(l));
            }
            if (lineas.size() > 1 && lineas.get(lineas.size() - 1).length() == 0) {
                lineas.remove(lineas.size() - 1);
            }
            repintar();
        }

        protected void pintar(Grafico g) {
            int filas = alto / 16;
            g.color(Colores.TEXTO);
            for (int i = 0; i < filas && primera + i < lineas.size(); i++) {
                g.texto(lineas.get(primera + i).toString(), 4, i * 16);
            }
            if (tieneFoco()) {
                int cx = 4 + col * 8, cy = (fila - primera) * 16;
                g.color(Colores.SELECCION);
                g.rectRelleno(cx, cy, 2, 16);
            }
        }

        protected void ratonPulsado(int px, int py, int boton) {
            fila = Math.min(lineas.size() - 1, primera + py / 16);
            col = Math.min(lineas.get(fila).length(), Math.max(0, (px - 4) / 8));
            repintar();
        }

        protected void teclaPulsada(int t, int mods) {
            StringBuilder l = lineas.get(fila);
            if (t == '\n') {
                StringBuilder resto = new StringBuilder(l.substring(col));
                l.setLength(col);
                lineas.add(++fila, resto);
                col = 0;
            } else if (t == '\b') {
                if (col > 0) {
                    l.deleteCharAt(--col);
                } else if (fila > 0) {
                    StringBuilder ant = lineas.get(fila - 1);
                    col = ant.length();
                    ant.append(l);
                    lineas.remove(fila--);
                }
            } else if (t == Eventos.SUPRIMIR) {
                if (col < l.length()) l.deleteCharAt(col);
            } else if (t == Eventos.IZQUIERDA) {
                if (col > 0) col--;
            } else if (t == Eventos.DERECHA) {
                if (col < l.length()) col++;
            } else if (t == Eventos.ARRIBA) {
                if (fila > 0) { fila--; col = Math.min(col, lineas.get(fila).length()); }
            } else if (t == Eventos.ABAJO) {
                if (fila < lineas.size() - 1) { fila++; col = Math.min(col, lineas.get(fila).length()); }
            } else if (t >= 0x20 && t < 0x100 && t != 0x7F) {
                l.insert(col++, (char) t);
            }
            int filas = alto / 16;
            if (fila < primera) primera = fila;
            if (fila >= primera + filas) primera = fila - filas + 1;
            repintar();
        }
    }

    static String leer(String ruta) throws IOException {
        BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(ruta), "UTF-8"));
        StringBuilder sb = new StringBuilder();
        String l;
        while ((l = r.readLine()) != null) {
            sb.append(l).append('\n');
        }
        r.close();
        return sb.toString();
    }

    static void escribir(String ruta, String texto) throws IOException {
        FileOutputStream f = new FileOutputStream(ruta);
        f.write(texto.getBytes("UTF-8"));
        f.close();
    }

    static String fichero = null;

    public static void main(String[] args) throws Exception {
        final Ventana v = new Ventana("Bloc de notas", 520, 360);
        final Editor ed = new Editor(4, 26, 512, 328);
        if (args.length > 0) {
            fichero = args[0];
            ed.cargar(leer(fichero));
        }
        Menu archivo = new Menu("Archivo");
        archivo.elemento("Nuevo", new Accion() {
            public void ejecutar(Componente o) { ed.nuevo(); fichero = null; }
        });
        archivo.elemento("Abrir...", new Accion() {
            public void ejecutar(Componente o) {
                String r = Dialogo.abrirFichero("Abrir", "Texto (*.txt)|*.txt|Todos|*", "/");
                if (r != null) {
                    try { ed.cargar(leer(r)); fichero = r; }
                    catch (IOException e) { Dialogo.mensaje("Error", "No se pudo leer " + r); }
                }
            }
        });
        archivo.elemento("Guardar", new Accion() {
            public void ejecutar(Componente o) {
                if (fichero == null) {
                    fichero = Dialogo.guardarFichero("Guardar", "Texto (*.txt)|*.txt", "/tmp/nota.txt");
                }
                if (fichero != null) {
                    try { escribir(fichero, ed.texto()); }
                    catch (IOException e) { Dialogo.mensaje("Error", "No se pudo guardar"); }
                }
            }
        });
        archivo.separador();
        archivo.elemento("Salir", new Accion() {
            public void ejecutar(Componente o) { v.cerrar(); }
        });
        BarraMenu barra = new BarraMenu();
        barra.agregar(archivo);
        v.agregar(ed);
        v.barraMenu(barra);
        v.enfocar(ed);
        v.ejecutar();
    }
}
