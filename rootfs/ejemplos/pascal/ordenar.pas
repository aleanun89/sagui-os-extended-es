{ ordenar.pas - arrays de records: ordena alumnos por nota (burbuja) y por
  nombre (inserción), y calcula la media.
    pascal /ejemplos/pascal/ordenar.pas -o /tmp/ordenar && /tmp/ordenar }
program Ordenar;
type
  Alumno = record
    nombre: string[12];
    nota: real;
  end;
  Clase = array[1..6] of Alumno;

const
  Lista: Clase = (
    (nombre: 'Lucía';  nota: 8.5),
    (nombre: 'Pedro';  nota: 6.25),
    (nombre: 'Ana';    nota: 9.75),
    (nombre: 'Tomás';  nota: 5.0),
    (nombre: 'Marta';  nota: 7.5),
    (nombre: 'Bruno';  nota: 4.75));

var
  c: Clase;

procedure mostrar(const titulo: string; const c: Clase);
var i: integer;
begin
  writeln(titulo);
  for i := 1 to 6 do
    writeln('  ', i:2, '. ', c[i].nombre:-12, c[i].nota:6:2)
end;

procedure por_nota(var c: Clase);
var i, j: integer;
    t: Alumno;
begin
  for i := 1 to 5 do
    for j := 1 to 6 - i do
      if c[j].nota < c[j + 1].nota then begin
        t := c[j];
        c[j] := c[j + 1];
        c[j + 1] := t
      end
end;

procedure por_nombre(var c: Clase);
var i, j: integer;
    t: Alumno;
begin
  for i := 2 to 6 do begin
    t := c[i];
    j := i - 1;
    while (j >= 1) and (c[j].nombre > t.nombre) do begin
      c[j + 1] := c[j];
      j := j - 1
    end;
    c[j + 1] := t
  end
end;

function media(const c: Clase): real;
var i: integer;
    s: real;
begin
  s := 0;
  for i := 1 to 6 do s := s + c[i].nota;
  media := s / 6
end;

begin
  c := Lista;
  por_nota(c);
  mostrar('Por nota:', c);
  por_nombre(c);
  mostrar('Por nombre:', c);
  writeln('Media: ', media(c):0:3)
end.
