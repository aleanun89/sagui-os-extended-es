-- Se borra cuando termina su animación (señal «animacion_terminada» del Sprite).
local Explosion = {}
function Explosion:_listo()
  self:conectar("animacion_terminada", function() self.fin = true end)
  self.t = 0
end
function Explosion:_proceso(dt)
  self.t = self.t + dt
  if self.t > 0.9 then self:liberar() end
end
return Explosion
