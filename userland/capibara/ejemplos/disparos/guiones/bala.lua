-- Disparo del jugador: sube; al tocar un enemigo le quita vida y desaparece.
local Bala = {}
function Bala:_listo()
  self.vx = self.vx or 0
  self:conectar("cuerpo_entro", self, "_al_tocar")
end
function Bala:_fisica(dt)
  self.y = self.y - 330 * dt
  self.x = self.x + self.vx * dt
  if self.y < -20 then self:liberar() end
end
function Bala:_al_tocar(enemigo)
  if self.gastada then return end
  self.gastada = true
  enemigo:danar(1)
  self:liberar()
end
return Bala
