package sagui.gui;

/**
 * Dialogo - ventanas de diálogo del sistema: mensaje, pregunta, pedir texto y elegir ficheros.
 * Esperan la respuesta (son modales). Los filtros de ficheros tienen la forma
 * "Texto (*.txt)|*.txt|Todos|*".
 */
public final class Dialogo {
    private Dialogo() { }

    public static void mensaje(String titulo, String texto) { Nativo.mensaje(titulo, texto); }

    /** Pregunta Sí/No. */
    public static boolean confirmar(String titulo, String pregunta) { return Nativo.confirmar(titulo, pregunta); }

    /** Pide una línea de texto; null si se cancela. */
    public static String pedirTexto(String titulo, String pregunta, String inicial) {
        return Nativo.pedirTexto(titulo, pregunta, inicial);
    }

    /** Diálogo de abrir fichero: la ruta elegida o null. */
    public static String abrirFichero(String titulo, String filtros, String carpetaInicial) {
        return Nativo.dialogoFichero(false, titulo, filtros == null ? "" : filtros, carpetaInicial);
    }

    /** Diálogo de guardar fichero: la ruta elegida o null. */
    public static String guardarFichero(String titulo, String filtros, String carpetaYNombre) {
        return Nativo.dialogoFichero(true, titulo, filtros == null ? "" : filtros, carpetaYNombre);
    }
}
