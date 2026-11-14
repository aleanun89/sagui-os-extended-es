{ fibonacci.pas - funciones recursivas y una tabla para no repetir cuentas.
    pascal /ejemplos/pascal/fibonacci.pas -o /tmp/fib && /tmp/fib }
program Fibonacci;
var
  memoria: array[0..46] of integer;
  i: integer;

{ La definición tal cual: lenta, porque repite los mismos cálculos. }
function fib(n: integer): integer;
begin
  if n < 2 then
    fib := n
  else
    fib := fib(n - 1) + fib(n - 2)
end;

{ La misma, recordando lo ya calculado. }
function fib_rapido(n: integer): integer;
begin
  if memoria[n] = 0 then
    if n < 2 then memoria[n] := n
    else memoria[n] := fib_rapido(n - 1) + fib_rapido(n - 2);
  fib_rapido := memoria[n]
end;

begin
  for i := 0 to 15 do write(fib(i), ' ');
  writeln;
  writeln('fib(25) = ', fib(25));
  writeln('fib(46) = ', fib_rapido(46), ' (el mayor que cabe en un integer)')
end.
