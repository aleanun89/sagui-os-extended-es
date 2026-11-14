{ lista.pas - punteros: una lista enlazada ordenada con new y dispose.
    pascal /ejemplos/pascal/lista.pas -o /tmp/lista && /tmp/lista }
program Lista;
type
  PNodo = ^Nodo;
  Nodo = record
    valor: integer;
    siguiente: PNodo;
  end;

var
  cabeza: PNodo;
  i: integer;

{ Inserta manteniendo la lista ordenada. }
procedure insertar(var l: PNodo; v: integer);
var nuevo: PNodo;
begin
  if (l = nil) or (v < l^.valor) then begin
    new(nuevo);
    nuevo^.valor := v;
    nuevo^.siguiente := l;
    l := nuevo
  end else
    insertar(l^.siguiente, v)
end;

procedure mostrar(l: PNodo);
begin
  write('[');
  while l <> nil do begin
    write(l^.valor);
    if l^.siguiente <> nil then write(', ');
    l := l^.siguiente
  end;
  writeln(']')
end;

function longitud(l: PNodo): integer;
begin
  if l = nil then longitud := 0
  else longitud := 1 + longitud(l^.siguiente)
end;

procedure borrar(var l: PNodo; v: integer);
var p: PNodo;
begin
  if l = nil then exit;
  if l^.valor = v then begin
    p := l;
    l := l^.siguiente;
    dispose(p)
  end else
    borrar(l^.siguiente, v)
end;

procedure liberar(var l: PNodo);
var p: PNodo;
begin
  while l <> nil do begin
    p := l;
    l := l^.siguiente;
    dispose(p)
  end
end;

begin
  cabeza := nil;
  for i := 1 to 10 do insertar(cabeza, (i * 37) mod 23);
  mostrar(cabeza);
  writeln('Elementos: ', longitud(cabeza));
  borrar(cabeza, 14);
  borrar(cabeza, 0);
  mostrar(cabeza);
  liberar(cabeza);
  writeln('Lista vacía: ', cabeza = nil)
end.
