package sagui.gui;

/**
 * Nativo - la interfaz nativa con la libsagui (ventanas, dibujo, imágenes, diálogos, fuentes).
 * Es de uso interno: los programas usan {@link Ventana}, {@link Lienzo} y demás componentes.
 * Las funciones están implementadas en C (userland/terceros/minijvm/plataforma/gui_nativos.c).
 * Los colores son 0xRRGGBB; los textos, Java normal (se convierten a Latin-1 al dibujar con la
 * fuente de mapa de bits, y se pasan en UTF-8 a las fuentes vectoriales).
 */
public final class Nativo {
    private Nativo() { }

    public static native int abrir(String titulo, int ancho, int alto);
    public static native void cerrar(int v);
    public static native void mostrar(int v);
    public static native void titulo(int v, String t);
    public static native int ancho(int v);
    public static native int alto(int v);
    /** Limita el dibujo a un rectángulo; ancho <= 0 quita el límite. */
    public static native void recorte(int v, int x, int y, int ancho, int alto);
    public static native void rect(int v, int x, int y, int ancho, int alto, int color);
    public static native void linea(int v, int x0, int y0, int x1, int y1, int color);
    public static native void elipse(int v, int x, int y, int ancho, int alto, int color, boolean relleno);
    /** fuente 0 = la de mapa de bits del sistema (8x16); si no, un identificador de {@link #abrirFuente}. */
    public static native void texto(int v, int x, int y, String t, int color, int fuente, int px);
    public static native int anchoTexto(String t, int fuente, int px);
    public static native int cargarImagen(String ruta);
    public static native int imagenAncho(int h);
    public static native int imagenAlto(int h);
    public static native void liberarImagen(int h);
    public static native void dibujarImagen(int v, int h, int x, int y, int ancho, int alto);
    public static native int abrirFuente(String ruta);
    public static native void cerrarFuente(int f);
    public static native String listarFuentes(String carpeta);
    /** Espera un evento hasta ms milisegundos; devuelve 1 y rellena salida[7] (tipo, x, y, botones, clics, tecla, modificadores). */
    public static native int evento(int v, int ms, int[] salida);
    public static native void mensaje(String titulo, String texto);
    public static native boolean confirmar(String titulo, String pregunta);
    public static native String pedirTexto(String titulo, String pregunta, String inicial);
    public static native String dialogoFichero(boolean guardar, String titulo, String filtros, String inicial);
    public static native void copiar(String texto);
    public static native String pegar();
    public static native int milisegundos();
}
