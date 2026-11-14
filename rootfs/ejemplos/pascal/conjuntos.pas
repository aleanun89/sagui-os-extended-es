{ conjuntos.pas - set of: unión, intersección, diferencia e "in".
    pascal /ejemplos/pascal/conjuntos.pas -o /tmp/conj && /tmp/conj }
program Conjuntos;
type
  Dia = (lunes, martes, miercoles, jueves, viernes, sabado, domingo);
  Dias = set of Dia;
const
  Nombres: array[Dia] of string[9] =
    ('lunes', 'martes', 'miércoles', 'jueves', 'viernes', 'sábado', 'domingo');
var
  trabajo, gimnasio, libres: Dias;
  d: Dia;
  vocales, letras: set of char;
  frase: string;
  i, nv, nc: integer;

procedure mostrar(const titulo: string; s: Dias);
var d: Dia;
begin
  write(titulo, ':');
  for d := lunes to domingo do
    if d in s then write(' ', Nombres[d]);
  writeln
end;

begin
  trabajo := [lunes..viernes];
  gimnasio := [martes, jueves, sabado];
  libres := [lunes..domingo] - trabajo;
  mostrar('Trabajo', trabajo);
  mostrar('Gimnasio', gimnasio);
  mostrar('Libres', libres);
  mostrar('Trabajo y gimnasio', trabajo * gimnasio);
  mostrar('Algo que hacer', trabajo + gimnasio);
  writeln('¿El gimnasio cabe en la semana laboral? ', gimnasio <= trabajo);

  vocales := ['a', 'e', 'i', 'o', 'u', 'á', 'é', 'í', 'ó', 'ú'];
  letras := ['a'..'z', 'á', 'é', 'í', 'ó', 'ú', 'ñ', 'ü'];
  frase := 'el murciélago comía ñame';
  nv := 0;
  nc := 0;
  for i := 1 to length(frase) do
    if frase[i] in vocales then nv := nv + 1
    else if frase[i] in letras then nc := nc + 1;
  writeln('"', frase, '": ', nv, ' vocales y ', nc, ' consonantes')
end.
