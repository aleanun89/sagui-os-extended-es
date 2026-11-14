{ fractal.pas - la unidad sagui: el conjunto de Mandelbrot en una ventana,
  con un marco, un círculo y un título. Una tecla para salir.
    pascal /ejemplos/pascal/fractal.pas -o /tmp/fractal && /tmp/fractal }
program Fractal;
uses sagui;
const
  Ancho = 320;
  Alto = 240;
  Vueltas = 48;
var
  px, py, n: integer;
  x0, y0, x, y, t: real;

{ Colores del azul oscuro al amarillo según las vueltas. }
function paleta(n: integer): integer;
begin
  if n >= Vueltas then paleta := negro
  else paleta := rgb(n * 5, n * 3 + 40, 255 - n * 4)
end;

begin
  abrir_ventana(Ancho, Alto, 'Mandelbrot en Pascal');
  for py := 0 to Alto - 1 do begin
    y0 := (py - Alto / 2) / (Alto / 2.4);
    for px := 0 to Ancho - 1 do begin
      x0 := (px - Ancho * 0.65) / (Ancho / 3.2);
      x := 0;
      y := 0;
      n := 0;
      while (n < Vueltas) and (x * x + y * y < 4) do begin
        t := x * x - y * y + x0;
        y := 2 * x * y + y0;
        x := t;
        n := n + 1
      end;
      color(paleta(n));
      pixel(px, py)
    end;
    if py mod 40 = 0 then actualizar
  end;
  color(blanco);
  rect(4, 4, Ancho - 8, Alto - 8);
  color(amarillo);
  circulo(Ancho - 40, 40, 20);
  circulo_lleno(Ancho - 40, 40, 6);
  color(rojo);
  linea(10, Alto - 10, Ancho - 10, Alto - 30);
  color(blanco);
  texto(10, 10, 'Mandelbrot - pulsa una tecla');
  esperar_tecla
end.
