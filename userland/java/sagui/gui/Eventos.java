package sagui.gui;

/** Eventos de ventana: los códigos son los del sistema (EV_* y TECLA_*). */
public final class Eventos {
    private Eventos() { }
    public static final int CLIC = 1, SOLTAR = 2, MOVER = 3, TECLA = 4, CERRAR = 5, REDIMENSIONAR = 10, RUEDA = 11;
    public static final int ARRIBA = 0x100, ABAJO = 0x101, IZQUIERDA = 0x102, DERECHA = 0x103, INICIO = 0x104,
            FIN = 0x105, REPAG = 0x106, AVPAG = 0x107, INSERTAR = 0x108, SUPRIMIR = 0x109, ESC = 0x116;
    public static final int MOD_MAYUS = 1, MOD_CTRL = 2, MOD_ALT = 4;
}
