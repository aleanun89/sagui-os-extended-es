program E2;
{$R+}
var v: array[1..5] of integer; i: integer;
begin
  for i := 1 to 6 do begin
    v[i] := i * i;
    writeln(v[i])
  end
end.
