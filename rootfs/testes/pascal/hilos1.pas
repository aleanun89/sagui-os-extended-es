{ Pruebas de la unidad Hilos (D171): crear y esperar, contador con mutex
  exacto, productor/consumidor con semaforo, New/Dispose y WriteLn desde
  varios hilos. Cada linea "OK ..." es una prueba pasada. }
program hilos1;
uses Hilos;

const
  N = 4;
  VUELTAS = 20000;
  ITEMS = 2000;

type
  PNodo = ^TNodo;
  TNodo = record
    v: integer;
    sig: PNodo
  end;

var
  m: TMutex;
  contador, i, fallos, r: integer;
  h: array[1..N] of THilo;
  hecho: array[1..N] of integer;
  lleno, vacio: TSemaforo;
  buffer: array[0..7] of integer;
  puesto, sacado: integer;
  suma_prod, suma_cons: integer;
  memo_malos: integer;

procedure Cuenta(arg: Pointer);
var k: integer;
begin
  for k := 1 to VUELTAS do begin
    TomarMutex(m);
    contador := contador + 1;
    SoltarMutex(m)
  end
end;

procedure Marca(arg: Pointer);
begin
  hecho[integer(arg)] := integer(arg) * 10;
  CederTurno;
  SalirHilo(integer(arg) + 100);
  hecho[integer(arg)] := -1      { no debe ejecutarse }
end;

procedure Productor(arg: Pointer);
var k: integer;
begin
  for k := 1 to ITEMS do begin
    TomarSemaforo(vacio);
    buffer[puesto mod 8] := k;
    puesto := puesto + 1;
    suma_prod := suma_prod + k;
    SoltarSemaforo(lleno)
  end
end;

procedure Consumidor(arg: Pointer);
var k: integer;
begin
  for k := 1 to ITEMS do begin
    TomarSemaforo(lleno);
    suma_cons := suma_cons + buffer[sacado mod 8];
    sacado := sacado + 1;
    SoltarSemaforo(vacio)
  end
end;

procedure Memoria(arg: Pointer);
var k: integer;
    p: PNodo;
begin
  for k := 1 to 2000 do begin
    New(p);
    p^.v := integer(arg) * 100000 + k;
    p^.sig := nil;
    CederTurno;
    if p^.v <> integer(arg) * 100000 + k then memo_malos := memo_malos + 1;
    Dispose(p)
  end
end;

procedure Escribe(arg: Pointer);
var k: integer;
begin
  for k := 1 to 3 do WriteLn('LINEA hilo ', integer(arg), ' vuelta ', k, ' fin')
end;

begin
  fallos := 0;
  { 1. crear y esperar }
  for i := 1 to N do h[i] := CrearHilo(@Marca, Pointer(i));
  for i := 1 to N do begin
    r := EsperarHilo(h[i]);
    if (r <> i + 100) or (hecho[i] <> i * 10) then fallos := fallos + 1
  end;
  if fallos = 0 then WriteLn('OK crear y esperar') else WriteLn('FALLA crear y esperar');
  { 2. contador con mutex }
  CrearMutex(m);
  contador := 0;
  for i := 1 to N do h[i] := CrearHilo(@Cuenta, nil);
  for i := 1 to N do EsperarHilo(h[i]);
  if contador = N * VUELTAS then WriteLn('OK contador ', contador)
  else begin WriteLn('FALLA contador ', contador); fallos := fallos + 1 end;
  { 3. productor/consumidor }
  CrearSemaforo(lleno, 0);
  CrearSemaforo(vacio, 8);
  h[1] := CrearHilo(@Productor, nil);
  h[2] := CrearHilo(@Consumidor, nil);
  EsperarHilo(h[1]);
  EsperarHilo(h[2]);
  if (suma_prod = suma_cons) and (suma_cons = ITEMS * (ITEMS + 1) div 2) then WriteLn('OK semaforo ', suma_cons)
  else begin WriteLn('FALLA semaforo ', suma_prod, ' ', suma_cons); fallos := fallos + 1 end;
  { 4. memoria dinamica desde varios hilos }
  memo_malos := 0;
  for i := 1 to N do h[i] := CrearHilo(@Memoria, Pointer(i));
  for i := 1 to N do EsperarHilo(h[i]);
  if memo_malos = 0 then WriteLn('OK memoria') else begin WriteLn('FALLA memoria ', memo_malos); fallos := fallos + 1 end;
  { 5. salida: las lineas no se mezclan }
  for i := 1 to N do h[i] := CrearHilo(@Escribe, Pointer(i));
  for i := 1 to N do EsperarHilo(h[i]);
  if IdHilo >= 0 then WriteLn('OK idhilo ', IdHilo > -1);
  WriteLn('RESULTADO hilos pascal: ', fallos, ' fallos')
end.
