{ bucle.pas - benchmark entero del generador (D108): criba, mezcla con
  divisiones y desplazamientos y producto de matrices. Imprime sumas de
  comprobacion (iguales con cualquier optimizacion) y el tiempo en ms. }
program bucle;
const N = 60000; REP = 400;
var
  criba: array[0..N] of boolean;
  ma, mb, mc: array[0..31, 0..31] of integer;
  tabla: array[0..63] of integer;
  t0, i, j, k, s, np, r, a: integer;

function hash(x: integer): integer;
begin
  x := x * 31 + (x shr 3);
  x := x xor (x shl 5);
  x := x mod 1000003;
  if x < 0 then x := -x;
  hash := x div 7 + x mod 7;
end;

begin
  t0 := __syscall(41, 0, 0, 0);
  s := 0; np := 0;
  for r := 0 to REP - 1 do begin
    for i := 0 to N do criba[i] := true;
    i := 2;
    while i * i <= N do begin
      if criba[i] then begin
        j := i * i;
        while j <= N do begin criba[j] := false; j := j + i; end;
      end;
      i := i + 1;
    end;
    np := 0;
    for i := 2 to N do if criba[i] then np := np + 1;
  end;
  writeln('primos=', np);
  for r := 0 to REP * 2 - 1 do
    for i := 0 to 19999 do begin
      s := s + (hash(i + r) and 1023);
      tabla[i and 63] := tabla[i and 63] + s mod 17 + (i div 8);
    end;
  writeln('hash=', s, ' t=', tabla[5] + tabla[63]);
  for i := 0 to 31 do for j := 0 to 31 do begin ma[i, j] := i + j; mb[i, j] := i - j; end;
  for r := 0 to REP - 1 do begin
    for i := 0 to 31 do
      for j := 0 to 31 do begin
        a := 0;
        for k := 0 to 31 do a := a + ma[i, k] * mb[k, j];
        mc[i, j] := a;
      end;
    s := s + mc[r and 31, r and 31] + mc[31, 0];
  end;
  writeln('matriz=', s);
  writeln('ms=', __syscall(41, 0, 0, 0) - t0);
end.
