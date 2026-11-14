{ hilos.pas - la misma cuenta repartida en varios hilos (D171).
  Compila y ejecuta con:
      pascal /ejemplos/hilos.pas -o /tmp/hilos
      /tmp/hilos 4
  Con varios procesadores (arranque con --smp) los hilos corren a la vez y
  el tiempo con 4 hilos se parece al de 1; con uno solo, se suman. }
program hilos;
uses Hilos;

const MaxHilos = 8;

var
  cerrojo: TMutex;
  total: integer;
  n, i: integer;
  h: array[1..MaxHilos] of THilo;

function Cuenta(hasta: integer): integer;
var i, s: integer;
begin
  s := 0;
  for i := 0 to hasta - 1 do s := s + i mod 7;
  Cuenta := s
end;

procedure Trabajo(arg: Pointer);
var r: integer;
begin
  r := Cuenta(20000000);
  TomarMutex(cerrojo);          { el total es de todos: con el cerrojo }
  total := total + r;
  WriteLn('hilo ', integer(arg), ': ', r);
  SoltarMutex(cerrojo)
end;

begin
  n := 4;
  if ParamCount > 0 then Val(ParamStr(1), n, i);
  if n < 1 then n := 1;
  if n > MaxHilos then n := MaxHilos;
  CrearMutex(cerrojo);
  for i := 1 to n do h[i] := CrearHilo(@Trabajo, Pointer(i - 1));
  for i := 1 to n do EsperarHilo(h[i]);
  WriteLn('total ', total, ' con ', n, ' hilos')
end.
