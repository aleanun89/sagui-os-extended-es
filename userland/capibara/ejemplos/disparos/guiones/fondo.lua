-- Desplaza el mar hacia abajo y lo repite: el mapa tiene dos vueltas iguales de 21 filas (336 píxeles).
local Fondo = {}
function Fondo:_proceso(dt)
  self.y = self.y + 34 * dt
  if self.y >= -16 then self.y = self.y - 336 end
end
return Fondo
