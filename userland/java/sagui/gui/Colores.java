package sagui.gui;

/** Colores del tema de SaguiOS (0xRRGGBB) y alguno más. */
public final class Colores {
    private Colores() { }
    public static final int FONDO = 0xF4F4F0, TEXTO = 0x1E2328, TEXTO_APAGADO = 0x6A737C, BORDE = 0x8A939C,
            BOTON = 0xE2E6EA, BOTON_PULSADO = 0xC4CBD2, DESTACADO = 0x0B7A3E, SELECCION = 0x2A6FD8, BLANCO = 0xFFFFFF,
            NEGRO = 0, ROJO = 0xD02020, VERDE = 0x20A040, AZUL = 0x2050D0, AMARILLO = 0xE0C020, GRIS = 0x808080;
    public static int rgb(int r, int g, int b) { return ((r & 255) << 16) | ((g & 255) << 8) | (b & 255); }
}
