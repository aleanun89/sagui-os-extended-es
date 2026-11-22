-- La partida: oleadas de enemigos, puntos y fin de juego. Es la raíz de la escena (Capibara.raiz()).
local Partida = {}

function Partida:_listo()
  self.puntos, self.oleada, self.fin = 0, 0, false
  self.nave = self:obtener_nodo("Nave")
  self.et_puntos, self.et_vidas = self:obtener_nodo("Puntos"), self:obtener_nodo("Vidas")
  self.mensaje, self.boton = self:obtener_nodo("Mensaje"), self:obtener_nodo("Reiniciar")
  self.disparos = 0
  print("CAPI_JUEGO listo")
end

-- crea una escena, le pone variables y la cuelga de la raíz
function Partida:crear(escena, x, y, vars)
  local n = self:instanciar(escena)
  if not n then return nil end
  n.x, n.y = x, y
  if vars then for k, v in pairs(vars) do n[k] = v end end
  self:agregar_hijo(n)
  n:teletransportar(x, y)
  return n
end

function Partida:disparar(x, y, vx)
  local b = self:crear("escenas/bala.esc", x, y, { vx = vx })
  self.disparos = self.disparos + 1
  if self.disparos % 2 == 1 then Audio.sonido("audio/disparo.wav", 0.5) end
  return b
end

function Partida:disparo_enemigo(x, y)
  local nave = self.nave
  local dx, dy = nave.x - x, nave.y - y
  local d = math.max(1, math.sqrt(dx * dx + dy * dy))
  self:crear("escenas/bala_enemiga.esc", x, y, { vx = dx / d * 110, vy = dy / d * 110 })
end

function Partida:explosion(x, y) self:crear("escenas/explosion.esc", x, y) end

function Partida:enemigo_destruido(x, y)
  self:explosion(x, y)
  Audio.sonido("audio/explosion.wav", 0.7)
  self.puntos = self.puntos + 100
  if math.random() < 0.18 then self:crear("escenas/mejora.esc", x, y, { clase = (math.random() < 0.4) and 1 or 0 }) end
  print("CAPI_JUEGO enemigo_destruido puntos=" .. self.puntos)
end

function Partida:_nueva_oleada()
  if self.fin then return end
  self.oleada = self.oleada + 1
  local patron = self.oleada % 3
  local n = 3 + math.min(4, self.oleada // 4)
  for i = 1, n do
    local x = 30 + (i - 1) * ((240 - 60) / math.max(1, n - 1))
    self:crear("escenas/enemigo.esc", x, -20 - (patron == 2 and i * 14 or 0), { patron = patron, modelo = 5 + (i % 3), vida = 2 + self.oleada // 6 })
  end
end

function Partida:perder()
  if self.fin then return end
  self.fin = true
  self.mensaje.texto = "Fin del juego"
  self.boton.visible = true
  print("CAPI_JUEGO derrota puntos=" .. self.puntos)
end

function Partida:_proceso(dt)
  self.et_puntos.texto = "Puntos: " .. self.puntos
  self.et_vidas.texto = "Vidas: " .. math.max(0, self.nave.vidas) .. "  Potencia: " .. self.nave.potencia
end
function Partida:_reiniciar() Capibara.cambiar_escena("escenas/partida.esc") end
return Partida
