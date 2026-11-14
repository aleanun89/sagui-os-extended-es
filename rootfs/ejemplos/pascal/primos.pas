{ primos.pas - criba de Eratóstenes (arrays de boolean y bucles).
    pascal /ejemplos/pascal/primos.pas -o /tmp/primos && /tmp/primos 100 }
program Primos;
const
  Maximo = 100000;
var
  compuesto: array[2..Maximo] of boolean;
  n, i, j, cuantos, codigo: integer;
begin
  n := 100;
  if paramcount >= 1 then val(paramstr(1), n, codigo);
  if n > Maximo then n := Maximo;
  cuantos := 0;
  for i := 2 to n do
    if not compuesto[i] then begin
      cuantos := cuantos + 1;
      write(i, ' ');
      j := i * 2;
      while j <= n do begin
        compuesto[j] := true;
        j := j + i
      end
    end;
  writeln;
  writeln(cuantos, ' primos hasta ', n)
end.
