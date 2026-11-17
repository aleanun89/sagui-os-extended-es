package sagui.gui;

/** Accion - lo que ocurre al pulsar un botón, elegir un menú, aceptar un campo o cambiar una casilla. */
public interface Accion {
    void ejecutar(Componente origen);
}
