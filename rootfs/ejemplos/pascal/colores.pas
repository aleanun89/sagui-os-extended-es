{ colores.pas - la unidad crt: clrscr, gotoxy, textcolor, readkey y delay.
  Mueve una estrella con las flechas; Esc o q para salir. (textcolor queda
  guardado: el terminal aún no muestra colores.)
    pascal /ejemplos/pascal/colores.pas -o /tmp/colores && /tmp/colores }
program Colores;
uses crt;
var
  x, y, c: integer;
  tecla: char;

procedure marco;
var i: integer;
begin
  textcolor(lightcyan);
  for i := 1 to 40 do begin
    gotoxy(i, 2); write('-');
    gotoxy(i, 14); write('-')
  end;
  normvideo
end;

begin
  clrscr;
  for c := 1 to 15 do begin
    textcolor(c);
    write('#', c, ' ')
  end;
  normvideo;
  marco;
  gotoxy(1, 16);
  write('Flechas: mover; q o Esc: salir');
  x := 20;
  y := 8;
  repeat
    gotoxy(x, y);
    textcolor(yellow);
    write('*');
    normvideo;
    tecla := readkey;
    gotoxy(x, y);
    write(' ');
    if tecla = #0 then
      case readkey of
        #72: if y > 3 then y := y - 1;
        #80: if y < 13 then y := y + 1;
        #75: if x > 1 then x := x - 1;
        #77: if x < 40 then x := x + 1;
      end
  until (tecla = 'q') or (tecla = #27);
  gotoxy(1, 18);
  writeln('¡Adiós!')
end.
