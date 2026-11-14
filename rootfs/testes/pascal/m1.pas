program Mezcla;
{$DEFINE PRUEBA}
type
  Figura = (circulo, rectangulo);
  Forma = record
    nombre: string[10];
    case tipo: Figura of
      circulo: (radio: real);
      rectangulo: (ancho, alto: integer);
  end;
  PForma = ^Forma;
  Vector = array[1..200] of integer;
  Letras = set of 'a'..'z';
  Par = record a, b: integer end;
var
  f: Forma; pf: PForma; v: Vector; i, n: integer; l: Letras; p: Par;
  nombres: array[1..4] of string[8];
  grande: array[0..999] of integer;

function area(const f: Forma): real;
begin
  case f.tipo of
    circulo: area := pi * sqr(f.radio);
    rectangulo: area := f.ancho * f.alto
  end
end;

function inverso(s: string): string;
var i: integer;
begin
  result := '';
  for i := length(s) downto 1 do result := result + s[i]
end;

function hacer_par(x, y: integer): Par;
begin
  hacer_par.a := x;
  result.b := y
end;

function vocales: Letras;
begin
  vocales := ['a', 'e', 'i', 'o', 'u']
end;

procedure quicksort(var a: Vector; lo, hi: integer);
var i, j, piv, t: integer;
begin
  i := lo; j := hi; piv := a[(lo + hi) div 2];
  repeat
    while a[i] < piv do inc(i);
    while a[j] > piv do dec(j);
    if i <= j then begin t := a[i]; a[i] := a[j]; a[j] := t; inc(i); dec(j) end
  until i > j;
  if lo < j then quicksort(a, lo, j);
  if i < hi then quicksort(a, i, hi)
end;

procedure contar;
const veces: integer = 0;
begin
  inc(veces);
  write(veces)
end;

begin
  f.nombre := 'disco'; f.tipo := circulo; f.radio := 2;
  writeln(f.nombre, ' ', area(f):0:3);
  new(pf); pf^.nombre := 'caja'; pf^.tipo := rectangulo; pf^.ancho := 3; pf^.alto := 4;
  with pf^ do writeln(nombre, ' ', area(pf^):0:1, ' ', ancho * alto);
  writeln(inverso('SaguiOS'), ' ', length(inverso('abc') + inverso('de')));
  p := hacer_par(7, 8); writeln(p.a, ' ', p.b, ' ', hacer_par(1, 2).b);
  l := vocales; writeln('e' in l, ' ', 'z' in l, ' ', 'a' in vocales);
  n := 200;
  for i := 1 to n do v[i] := (i * 7919) mod 1000;
  quicksort(v, 1, n);
  for i := 1 to 10 do write(v[i], ' '); writeln(v[n]);
  for i := 1 to 3 do contar; writeln;
  nombres[1] := 'zeta'; nombres[2] := 'alfa'; nombres[3] := 'mu'; nombres[4] := 'beta';
  for i := 1 to 3 do for n := i + 1 to 4 do
    if nombres[n] < nombres[i] then begin f.nombre := nombres[i]; nombres[i] := nombres[n]; nombres[n] := f.nombre end;
  for i := 1 to 4 do write(nombres[i], ' '); writeln;
  for i := 0 to 999 do grande[i] := i; n := 0; for i := 0 to 999 do n := n + grande[i]; writeln(n);
  {$IFDEF PRUEBA} writeln('definido'); {$ELSE} writeln('no'); {$ENDIF}
  {$IFNDEF PRUEBA} writeln('mal'); {$ENDIF}
  i := maxint; i := i + 1; writeln(i);
  writeln(high(integer), ' ', low(byte), ' ', high(char) = #255, ' ', sizeof(Forma));
end.
