program T1;
const N = 10; M = N * 2 + 1; Saludo = 'hola';
var i, j, s: integer; b: byte; c: char; ok: boolean; r: real;
function fib(n: integer): integer;
begin
  if n < 2 then fib := n else fib := fib(n - 1) + fib(n - 2)
end;
procedure cuenta(var x: integer; paso: integer);
begin
  x := x + paso
end;
begin
  s := 0;
  for i := 1 to N do s := s + i;
  writeln('suma=', s, ' M=', M);
  writeln('fib(20)=', fib(20));
  j := 5; cuenta(j, 3); writeln('j=', j);
  b := 200; b := b + 50; writeln('b=', b);
  c := 'A'; writeln(c, succ(c), chr(ord(c) + 2));
  ok := (s > 50) and not (j = 0); writeln(ok, ' ', not ok);
  writeln(17 div 5, ' ', 17 mod 5, ' ', -17 div 5, ' ', -17 mod 5);
  writeln(i:5, '|', s:-3, '|', 'x':3, '|');
  r := 3.14159; writeln(r:8:3, ' ', r * 2:0:2, ' ', 1/3:10:6);
  writeln(Saludo, ' ', length(Saludo), ' ', Saludo + ' mundo');
  writeln(r);
  writeln(-r);
  writeln(sqrt(2.0):0:10, ' ', sqr(7), ' ', abs(-5), ' ', abs(-2.5):0:1);
  writeln(trunc(7.9), ' ', round(7.5), ' ', round(-7.5), ' ', round(2.4));
  writeln(odd(3), ' ', ord('a'), ' ', 1 shl 10, ' ', 1024 shr 3, ' ', 5 xor 3);
  i := 0;
  while i < 3 do begin write(i, ' '); i := i + 1 end; writeln;
  repeat i := i - 1; write(i) until i = 0; writeln;
  for i := 5 downto 1 do write(i); writeln;
end.
