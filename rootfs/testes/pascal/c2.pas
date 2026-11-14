{ c2.pas - prueba de la unidad crt: readkey (teclas especiales con #0), delay y keypressed. }
program C2;
uses crt;
var c: char;
begin
  if keypressed then writeln('hay tecla') else writeln('sin tecla');
  repeat
    c := readkey;
    if c = #0 then writeln('especial ', ord(readkey))
    else writeln('tecla ', ord(c))
  until c = 'q';
  delay(200);
  writeln('fin')
end.
