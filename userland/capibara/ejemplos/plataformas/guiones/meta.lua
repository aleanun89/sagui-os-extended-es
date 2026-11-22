-- La bandera de la meta: al entrar el jugador, el nivel está completado.
local Meta = {}
function Meta:_listo()
  self:conectar("cuerpo_entro", function(cuerpo)
    if cuerpo:en_grupo("jugador") then Capibara.raiz():ganar() end
  end)
end
return Meta
