-- Una moneda: avisa al nivel cuando el jugador la toca y se borra.
local Moneda = {}
function Moneda:_listo()
  self:conectar("cuerpo_entro", self, "_al_entrar")   -- señal del Area: (nodo que entra)
end
function Moneda:_al_entrar(cuerpo)
  if cuerpo:en_grupo("jugador") then
    Audio.sonido("audio/moneda.wav", 0.8)
    local x, y = self:posicion_global()
    Capibara.raiz():moneda_cogida(x, y)
    self:liberar()
  end
end
return Moneda
