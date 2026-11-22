-- La nave del jugador: se mueve con las acciones izquierda/derecha/arriba/abajo y dispara con «disparar».
local Nave = {}
local VELOCIDAD, ESPERA = 150, 0.16

function Nave:_listo()
  self.espera, self.vidas, self.inv, self.potencia, self.muerta = 0, 3, 0, 1, false
  self.dibujo = self:obtener_nodo("Dibujo")
end

function Nave:herir()
  if self.inv > 0 or self.muerta then return end
  self.vidas = self.vidas - 1
  self.inv = 1.5
  Audio.sonido("audio/impacto.wav", 0.8)
  Capibara.raiz():explosion(self.x, self.y)
  print("CAPI_JUEGO herido vidas=" .. self.vidas)
  if self.vidas <= 0 then self.muerta = true; self.dibujo.visible = false; Capibara.raiz():perder() end
end

function Nave:mejorar(tipo)
  Audio.sonido("audio/mejora.wav", 0.8)
  if tipo == 1 then self.vidas = math.min(5, self.vidas + 1) else self.potencia = math.min(3, self.potencia + 1) end
  print("CAPI_JUEGO mejora tipo=" .. tipo)
end

function Nave:_fisica(dt)
  if self.muerta then return end
  local vx = Entrada.eje("izquierda", "derecha") * VELOCIDAD
  local vy = Entrada.eje("arriba", "abajo") * VELOCIDAD
  self:mover_y_deslizar(vx, vy)
  self.x = math.max(16, math.min(224, self.x))
  self.y = math.max(20, math.min(300, self.y))
  self.espera = self.espera - dt
  if Entrada.presionada("disparar") and self.espera <= 0 then
    self.espera = ESPERA / self.potencia
    local raiz = Capibara.raiz()
    raiz:disparar(self.x, self.y - 16, 0)
    if self.potencia >= 2 then raiz:disparar(self.x - 8, self.y - 10, -40); raiz:disparar(self.x + 8, self.y - 10, 40) end
  end
end

function Nave:_proceso(dt)
  if self.inv > 0 then self.inv = self.inv - dt; self.dibujo.visible = math.floor(self.inv * 14) % 2 == 0
  elseif not self.muerta then self.dibujo.visible = true end
end
return Nave
