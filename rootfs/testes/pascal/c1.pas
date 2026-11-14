{ c1.pas - prueba de la unidad crt: clrscr, gotoxy hacia arriba, wherex/wherey. }
program C1;
uses crt;
var i: integer;
begin
  clrscr;
  for i := 1 to 5 do begin
    gotoxy(10, 6 - i);
    write('fila', 6 - i)
  end;
  gotoxy(3, 1); write('AB');
  gotoxy(1, 7); write('x=', wherex, ' y=', wherey);
  textcolor(yellow); write(' color'); normvideo;
  gotoxy(12, 3); clreol; write('-');
  gotoxy(1, 8); writeln('fin crt')
end.
