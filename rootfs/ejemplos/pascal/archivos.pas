{ archivos.pas - archivos de texto: escribe una tabla, la vuelve a leer y
  suma las columnas.
    pascal /ejemplos/pascal/archivos.pas -o /tmp/arch && /tmp/arch }
program Archivos;
var
  f: text;
  i, a, b, suma_a, suma_b, lineas: integer;
  titulo: string;
begin
  assign(f, '/tmp/tabla.txt');
  rewrite(f);
  writeln(f, 'Tabla de cuadrados y cubos');
  for i := 1 to 10 do writeln(f, i * i, ' ', i * i * i);
  close(f);
  writeln('Escrito /tmp/tabla.txt');

  reset(f);
  readln(f, titulo);
  writeln('Título: ', titulo);
  suma_a := 0;
  suma_b := 0;
  lineas := 0;
  while not eof(f) do begin
    readln(f, a, b);
    suma_a := suma_a + a;
    suma_b := suma_b + b;
    lineas := lineas + 1
  end;
  close(f);
  writeln(lineas, ' líneas; suma de cuadrados = ', suma_a, ', de cubos = ', suma_b);

  append(f);
  writeln(f, 'fin');
  close(f);
  reset(f);
  lineas := 0;
  while not eof(f) do begin
    readln(f, titulo);
    lineas := lineas + 1
  end;
  close(f);
  writeln('Con la línea añadida: ', lineas, ' líneas; la última es "', titulo, '"')
end.
