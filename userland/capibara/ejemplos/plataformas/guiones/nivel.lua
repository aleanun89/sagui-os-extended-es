-- El nivel: cuenta monedas, enseña los textos y decide cuándo se gana o se pierde.
-- Este nodo es la raíz de la escena: Capibara.raiz() lo devuelve desde cualquier guion.
local Nivel = {}

function Nivel:_listo()
  self.monedas, self.fin = 0, false
  self.puntos, self.vidas_et = self:obtener_nodo("Puntos"), self:obtener_nodo("Vidas")
  self.mensaje, self.boton = self:obtener_nodo("Mensaje"), self:obtener_nodo("Reiniciar")
  self.jugador = self:obtener_nodo("Mundo/Jugador")
  self:actualizar()
  print("CAPI_JUEGO listo monedas_total=" .. self.monedas_total)
end

function Nivel:actualizar()
  self.puntos.texto = "Monedas: " .. self.monedas .. "/" .. self.monedas_total
  self.vidas_et.texto = "Vidas: " .. math.max(0, self.jugador.vidas)
end

function Nivel:chispas(x, y)
  local p = self:instanciar("escenas/chispas.esc")
  self:obtener_nodo("Mundo"):agregar_hijo(p)
  p:teletransportar(x, y)
end

function Nivel:moneda_cogida(x, y)
  self.monedas = self.monedas + 1
  self:chispas(x, y)
  self:actualizar()
  print("CAPI_JUEGO moneda " .. self.monedas .. "/" .. self.monedas_total)
end

function Nivel:enemigo_derrotado(x, y)
  self:chispas(x, y)
  print("CAPI_JUEGO enemigo_derrotado")
end

function Nivel:terminar(texto)
  self.fin = true
  self.mensaje.texto = texto
  self.boton.visible = true
  self:actualizar()
end

function Nivel:ganar()
  if self.fin then return end
  Audio.sonido("audio/victoria.wav", 0.8)
  self:terminar("¡Nivel completado!")
  print("CAPI_JUEGO victoria monedas=" .. self.monedas)
end

function Nivel:perder()
  if self.fin then return end
  self:terminar("Fin del juego")
  print("CAPI_JUEGO derrota")
end

function Nivel:_proceso(dt) if not self.fin then self:actualizar() end end
function Nivel:_reiniciar() Capibara.cambiar_escena("escenas/nivel1.esc") end
return Nivel
