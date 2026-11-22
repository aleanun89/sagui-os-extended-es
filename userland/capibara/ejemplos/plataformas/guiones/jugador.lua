-- El personaje del ejemplo: correr, saltar (con «tiempo de coyote» y salto variable) y recibir daño.
-- self es el nodo (un Cuerpo cinemático): self.vx y self.vy son su velocidad; lo demás son variables propias.
local Jugador = {}

local VELOCIDAD, SALTO, CAIDA_MAX, COYOTE, INVULNERABLE = 95, -300, 420, 0.10, 1.2

function Jugador:_listo()
  self.vidas, self.coyote, self.inv, self.muerto = 3, 0, 0, false
  self.dibujo = self:obtener_nodo("Dibujo")
end

function Jugador:herir(desde_x)
  if self.inv > 0 or self.muerto then return end
  self.vidas = self.vidas - 1
  self.inv = INVULNERABLE
  self.vy = -170
  self.vx = (self.x < desde_x) and -110 or 110
  self.empuje = 0.25
  Audio.sonido("audio/golpe.wav", 0.8)
  print("CAPI_JUEGO herido vidas=" .. self.vidas)
  if self.vidas <= 0 then self.muerto = true; Capibara.raiz():perder() end
end

function Jugador:rebotar() self.vy = -210 end

function Jugador:_fisica(dt)
  if self.muerto then return end
  local dir = Entrada.eje("izquierda", "derecha")
  local vx, vy = self.vx, math.min(self.vy + Fisica.gravedad() * dt, CAIDA_MAX)
  if (self.empuje or 0) > 0 then self.empuje = self.empuje - dt else vx = dir * VELOCIDAD end
  if self.en_suelo then self.coyote = COYOTE else self.coyote = self.coyote - dt end
  if Entrada.recien_presionada("saltar") and self.coyote > 0 then
    vy = SALTO; self.coyote = 0
    Audio.sonido("audio/salto.wav", 0.6)
    print("CAPI_JUEGO salto")
  end
  if Entrada.recien_soltada("saltar") and vy < -110 then vy = -110 end   -- salto más corto si se suelta
  self.vx, self.vy = self:mover_y_deslizar(vx, vy)
  self.dir = (dir ~= 0) and dir or self.dir
  if self.y > 330 then self:herir_vacio() end
end

function Jugador:herir_vacio()
  self.vidas = self.vidas - 1
  print("CAPI_JUEGO caida vidas=" .. self.vidas)
  if self.vidas <= 0 then self.muerto = true; Capibara.raiz():perder(); return end
  self:teletransportar(30, 170); self.vx, self.vy, self.inv = 0, 0, INVULNERABLE
end

function Jugador:_proceso(dt)
  local d = self.dibujo
  if self.inv > 0 then self.inv = self.inv - dt; d.visible = math.floor(self.inv * 12) % 2 == 0 else d.visible = true end
  if self.dir then d.espejo_h = self.dir < 0 end
  if not self.en_suelo then d.fotograma = 1; d:detener_anim()
  elseif math.abs(self.vx) > 5 or Entrada.eje("izquierda", "derecha") ~= 0 then d:animar("andar")
  else d:animar("quieto") end
end
return Jugador
