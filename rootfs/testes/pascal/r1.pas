program Reales;
var x, y: real; i: integer; a: array[1..3] of real;
function media(a, b: real): real; begin media := (a + b) / 2 end;
begin
  x := 1.5; y := 2;
  writeln(x + y:0:2, ' ', x - y:0:2, ' ', x * y:0:2, ' ', x / y:0:4, ' ', y / 3:0:6);
  writeln(media(3, 4):0:1, ' ', media(x, i):0:2);
  i := 7; x := i / 2; writeln(x:0:1, ' ', i * 1.5:0:2, ' ', 10 - x:0:1);
  writeln(sin(0):0:4, ' ', cos(0):0:4, ' ', arctan(1) * 4:0:8, ' ', pi:0:10);
  writeln(ln(1):0:3, ' ', exp(1):0:6, ' ', exp(ln(10)):0:6, ' ', sqrt(16):0:1);
  writeln(x > y, ' ', x = 3.5, ' ', i < x, ' ', -x:6:2);
  a[1] := 0.1; a[2] := 0.2; a[3] := a[1] + a[2];
  writeln(a[3]:0:17, ' ', a[3] = 0.3);
  writeln(1e10:0:0, ' ', 123456789.0 * 1000:0:0, ' ', 2.5e-3:0:5);
  writeln(trunc(-3.7), ' ', round(3.49999), ' ', round(-0.5), ' ', frac(3.25):0:2, ' ', int(-3.75):0:1);
  writeln(1.0, ' ', 0.0, ' ', -123.456);
  writeln(1e300 * 10:12, ' ', 12345.678:12);
  x := 0; for i := 1 to 10 do x := x + 0.1; writeln(x:0:15);
  writeln(sqr(1.5):0:2, ' ', abs(-1e-3):0:3, ' ', random(1), ' ', 7 / 7:0:1);
end.
