import java.util.*;

/* Colecciones.java - ArrayList, HashMap, TreeMap, ordenación y StringBuilder.
 *
 *   java -cp /apps/java/ejemplos Colecciones */
public class Colecciones {
    public static void main(String[] args) {
        List<Integer> lista = new ArrayList<Integer>();
        for (int i = 10; i >= 1; i--) {
            lista.add(i * i);
        }
        Collections.sort(lista);
        System.out.println("ordenada: " + lista);

        Map<String, Integer> contar = new TreeMap<String, Integer>();
        String[] palabras = "uno dos tres dos tres tres".split(" ");
        for (String p : palabras) {
            Integer c = contar.get(p);
            contar.put(p, c == null ? 1 : c + 1);
        }
        System.out.println("frecuencias: " + contar);

        Map<Integer, String> mapa = new HashMap<Integer, String>();
        for (int i = 0; i < 100; i++) {
            mapa.put(i, "v" + i);
        }
        System.out.println("hashmap: " + mapa.size() + " elementos, 42 -> " + mapa.get(42));

        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 5; i++) {
            sb.append(i).append(',');
        }
        sb.setLength(sb.length() - 1);
        System.out.println("sb: " + sb);

        int suma = 0;
        for (int x : lista) {
            suma += x;
        }
        System.out.println("suma de cuadrados 1..10 = " + suma);
    }
}
