{ enteros.pas - prueba del generador (D108): aritmetica entera, condiciones y
  vectores. La salida debe ser igual con pascal -O0 y -O1. }
program enteros;
type
  par = record x, y: integer; c: char; v: array[0..3] of integer; end;
var
  ga: array[0..15] of integer;
  gn: integer;
  gp: par;
  gps: array[0..2] of par;
  vals: array[0..28] of integer;
  i, j, k: integer;
  calls: integer;

function fdiv(x: integer): integer;
var r: integer;
begin
  r := 0;
  r := r + x div 1; r := r + x div 2; r := r + x div 4; r := r + x div 8; r := r + x div 16; r := r + x div 1024;
  r := r + x mod 1; r := r + x mod 2; r := r + x mod 4; r := r + x mod 8; r := r + x mod 16; r := r + x mod 1024;
  r := r + x div 3; r := r + x mod 3; r := r + x div 10; r := r + x mod 10; r := r + x div (-4); r := r + x mod (-4);
  fdiv := r;
end;

function fmul(x: integer): integer;
var r: integer;
begin
  r := 0;
  r := r + x * 0; r := r + x * 1; r := r + x * 2; r := r + x * 3; r := r + x * 4; r := r + x * 5; r := r + x * 8;
  r := r + x * 9; r := r + x * 10; r := r + x * 100; r := r + x * (-1); r := r + x * (-8); r := r + x * 200;
  r := r + 3 * x; r := r + 7 * x; r := r + 0 * x; r := r + 1 * x;
  r := r + x + 0; r := r + (x - 0); r := r + (x or 0); r := r + (x xor 0); r := r + (x and (-1)); r := r + (x and 0);
  r := r + (x shl 0); r := r + (x shr 0); r := r + (x shl 3); r := r + (x shr 2);
  r := r + (x and 255); r := r + (x or 255); r := r + (x xor 21845);
  r := r + (x and 100000); r := r + (x + 100000); r := r + (x - 100000); r := r + x * 100000;
  fmul := r;
end;

function fcmp(x, y: integer): integer;
var r: integer;
begin
  r := 0;
  if x < y then r := r + 1;
  if x <= y then r := r + 2;
  if x > y then r := r + 4;
  if x >= y then r := r + 8;
  if x = y then r := r + 16;
  if x <> y then r := r + 32;
  if x < 5 then r := r + 64;
  if 5 < x then r := r + 128;
  if 5 >= x then r := r + 256;
  if x = 7 then r := r + 512;
  if 7 <> x then r := r + 1024;
  if 3 > y then r := r + 2048;
  if gn < x then r := r + 4096;
  if x < gn then r := r + 8192;
  if x = gn then r := r + 16384;
  if (x < y) and (x >= gn) then r := r + 100000;
  fcmp := r;
end;

function flogico(a, b, c: integer): integer;
var r, w, n: integer;
    p, q, s: boolean;
begin
  r := 0;
  p := a > 0; q := b > 0; s := c > 0;
  if p and q then r := r + 1;
  if p or q then r := r + 2;
  if p and q and s then r := r + 4;
  if p or q or s then r := r + 8;
  if p and (q or s) then r := r + 16;
  if (p or q) and s then r := r + 32;
  if (p and q) or s then r := r + 64;
  if p or (q and s) then r := r + 128;
  if not (p and q) then r := r + 256;
  if not (p or q) then r := r + 512;
  if ((a > 1) or (b < 2)) and ((c <> 3) or (a = b)) then r := r + 1024;
  if ((a > 0) and (b > 0) and (c > 0)) or ((a < 0) and (b < 0)) then r := r + 2048;
  if p and q then r := r + 10000 else r := r + 777;
  w := 0;
  while (a > 0) and (b > 0) do begin a := a - 1; b := b - 1; w := w + 1; end;
  r := r + w * 7;
  w := 0;
  while (a < 3) or (b < 3) do begin a := a + 1; b := b + 1; w := w + 1; if w > 10 then break; end;
  r := r + w * 13;
  n := 0;
  repeat n := n + 1; until (n > 5) or (n * n > 20);
  r := r + n;
  flogico := r;
end;

function fvec(n: integer): integer;
var loc: array[0..9] of integer;
    m2: array[0..2, 0..3] of integer;
    i, j, s: integer;
begin
  s := 0;
  for i := 0 to 9 do loc[i] := i * i + n;
  for i := 0 to 15 do ga[i] := i * 3 - n;
  for i := 0 to 9 do s := s + loc[i] * 3 + ga[i];
  s := s + loc[3] + loc[9] + ga[0] + ga[15];
  loc[2] := 5; ga[3] := 9; loc[4] := loc[4] + 3; ga[5] := ga[5] - 2;
  s := s + loc[2] + ga[3] + loc[4] + ga[5];
  for i := 0 to 2 do for j := 0 to 3 do m2[i, j] := i * 10 + j + n;
  for i := 0 to 2 do for j := 0 to 3 do s := s + m2[i, j] * (i + 1) + m2[2, 3] + m2[1, j];
  fvec := s;
end;

function frec(n: integer): integer;
var l: par;
    i, s: integer;
begin
  l.x := n; l.y := n * 2; l.c := chr(65 + (n mod 20 + 20) mod 20);
  for i := 0 to 3 do l.v[i] := i + n;
  gp.x := l.y; gp.y := l.x + 1; gp.c := 'a';
  for i := 0 to 3 do gp.v[i] := l.v[i] * 2;
  gps[1].x := 10; gps[2].y := 11; gps[0].v[3] := 12; gps[1].v[1] := n;
  s := l.x + l.y + l.v[2] + gp.x + gp.y + gp.v[3] + ord(l.c);
  s := s + gps[1].x + gps[2].y + gps[0].v[3] + gps[1].v[1];
  inc(l.x); inc(l.v[0], 2); dec(gp.y);
  s := s + l.x + l.v[0] + gp.y;
  frec := s;
end;

function fchain(x: integer): integer;
var r: integer;
begin
  r := x;
  r := r + 1 + 2 + 3;
  r := r * 2 * 2 * 2;
  r := r - 1 - 1;
  r := (r + 3) div 4 + (r - 3) mod 4;
  if x > 0 then r := r + 5 else r := -r;
  r := r div 2 + r div 4 + r div 8 + r mod 8;
  fchain := r;
end;

function efecto(v: integer): boolean;
begin
  calls := calls + 1;
  efecto := v <> 0;
end;

function fcorto(a: integer): integer;
var r: integer;
begin
  r := 0;
  calls := 0;
  if efecto(a) and efecto(a - 1) then r := r + 1;
  if efecto(a) or efecto(a - 1) then r := r + 2;
  if efecto(0) and efecto(1) then r := r + 4;
  if efecto(1) or efecto(1) then r := r + 8;
  fcorto := r * 100 + calls;
end;

begin
  gn := 7;
  k := 0;
  for i := 0 to 28 do begin
    case i mod 12 of
      0: vals[i] := 0;
      1: vals[i] := 1;
      2: vals[i] := -1;
      3: vals[i] := 7 + i;
      4: vals[i] := -7 - i;
      5: vals[i] := 1023 + i * 3;
      6: vals[i] := -1025 - i;
      7: vals[i] := 65535 + i;
      8: vals[i] := -65536 - i;
      9: vals[i] := 123456789 - i * 1000;
      10: vals[i] := -123456789 + i * 7;
      11: vals[i] := 2147483000 + i;
    end;
  end;
  for i := 0 to 28 do begin
    writeln('v=', vals[i], ' div=', fdiv(vals[i]), ' mul=', fmul(vals[i]), ' chain=', fchain(vals[i] mod 1000));
    writeln('vec=', fvec(vals[i] mod 100), ' rec=', frec(vals[i] mod 100), ' corto=', fcorto(vals[i] mod 3));
  end;
  i := 0;
  while i < 29 do begin
    j := 0;
    while j < 29 do begin
      writeln(vals[i], ',', vals[j], ' cmp=', fcmp(vals[i], vals[j]), ' lg=',
              flogico(vals[i] mod 5, vals[j] mod 5, (vals[i] + vals[j]) mod 5));
      j := j + 3;
    end;
    i := i + 2;
  end;
end.
