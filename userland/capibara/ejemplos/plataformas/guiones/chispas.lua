-- Borra el nodo cuando las partículas ya han muerto.
local Chispas = {}
function Chispas:_listo() self.t = 0 end
function Chispas:_proceso(dt)
  self.t = self.t + dt
  if self.t > 1.2 then self:liberar() end
end
return Chispas
