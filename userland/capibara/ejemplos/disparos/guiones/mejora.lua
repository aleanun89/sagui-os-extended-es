-- Mejora: cae despacio; al tocarla la nave, da una vida o más potencia.
local Mejora = {}
function Mejora:_listo()
  self.clase = self.clase or 0
  self:conectar("cuerpo_entro", self, "_al_tocar")
  self:obtener_nodo("Dibujo").fotograma = self.clase == 1 and 24 or 25
end
function Mejora:_fisica(dt)
  self.y = self.y + 50 * dt
  if self.y > 340 then self:liberar() end
end
function Mejora:_al_tocar(nave)
  nave:mejorar(self.clase)
  self:liberar()
end
return Mejora
