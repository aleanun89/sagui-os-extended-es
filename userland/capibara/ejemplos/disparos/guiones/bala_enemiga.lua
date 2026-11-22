-- Disparo enemigo: baja con la velocidad que le dé quien lo crea (self.vx, self.vy).
local BalaEnemiga = {}
function BalaEnemiga:_listo()
  self.vx, self.vy = self.vx or 0, self.vy or 120
  self:conectar("cuerpo_entro", self, "_al_tocar")
end
function BalaEnemiga:_fisica(dt)
  self.x = self.x + self.vx * dt
  self.y = self.y + self.vy * dt
  if self.y > 340 or self.y < -20 or self.x < -20 or self.x > 260 then self:liberar() end
end
function BalaEnemiga:_al_tocar(nave)
  nave:herir()
  self:liberar()
end
return BalaEnemiga
