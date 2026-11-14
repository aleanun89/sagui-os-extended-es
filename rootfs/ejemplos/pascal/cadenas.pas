{ cadenas.pas - strings al estilo Turbo Pascal: copy, pos, delete, insert,
  upcase, str y val.
    pascal /ejemplos/pascal/cadenas.pas -o /tmp/cad && /tmp/cad }
program Cadenas;
var
  s, palabra: string;
  corta: string[8];
  i, n, codigo: integer;
  x: real;

{ Dentro de la función, "al_reves" en una expresión sería una llamada
  recursiva: el texto se va formando en una variable local. }
function al_reves(const s: string): string;
var i: integer;
    r: string;
begin
  r := '';
  for i := length(s) downto 1 do r := r + s[i];
  al_reves := r
end;

function mayusculas(s: string): string;
var i: integer;
begin
  for i := 1 to length(s) do s[i] := upcase(s[i]);
  mayusculas := s
end;

function es_palindromo(const s: string): boolean;
var limpia: string;
    i: integer;
begin
  limpia := '';
  for i := 1 to length(s) do
    if s[i] <> ' ' then limpia := limpia + upcase(s[i]);
  es_palindromo := limpia = al_reves(limpia)
end;

begin
  s := 'Pascal en SaguiOS';
  writeln(s, ' tiene ', length(s), ' letras');
  writeln('Al revés: ', al_reves(s));
  writeln('Mayúsculas: ', mayusculas(s));
  writeln('copy(s, 11, 7) = ', copy(s, 11, 7));
  writeln('pos(''en'', s) = ', pos('en', s));
  delete(s, 7, 3);
  writeln('Sin "en ": ', s);
  insert('Turbo ', s, 1);
  writeln('Con "Turbo ": ', s);
  corta := 'demasiado largo';
  writeln('string[8]: ', corta);

  { contar palabras }
  s := '  uno dos   tres cuatro ';
  n := 0;
  i := 1;
  while i <= length(s) do begin
    while (i <= length(s)) and (s[i] = ' ') do i := i + 1;
    if i <= length(s) then begin
      palabra := '';
      while (i <= length(s)) and (s[i] <> ' ') do begin
        palabra := palabra + s[i];
        i := i + 1
      end;
      n := n + 1;
      write('[', palabra, ']')
    end
  end;
  writeln(' = ', n, ' palabras');

  writeln(es_palindromo('Dabale arroz a la zorra el abad'));
  str(2026, s);
  writeln('str: "', s, '"');
  str(3.14159:8:3, s);
  writeln('str real: "', s, '"');
  val('1234', n, codigo);
  writeln('val: ', n + 1, ' (código ', codigo, ')');
  val('12,5', x, codigo);
  writeln('val con error en la posición ', codigo)
end.
