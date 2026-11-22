-- Un limo: patrulla, da la vuelta ante paredes y bordes, y se aplasta saltándole encima.
local Limo = {}
local VEL = 28

function Limo:_listo()
  self.dir = -1
  self.dano = self:obtener_nodo("Dano")
  self.dibujo = self:obtener_nodo("Dibujo")
end

function Limo:_fisica(dt)
  local vx, vy = self.dir * VEL, self.vy + Fisica.gravedad() * dt
  vx, vy = self:mover_y_deslizar(vx, vy)
  self.vy = vy
  local x, y = self:posicion_global()
  -- pared delante, o vacío bajo el pie delantero: se da la vuelta
  if self.en_pared or (self.en_suelo and not Fisica.punto_solido(x + self.dir * 9, y + 12)) then self.dir = -self.dir end
  self.dibujo.espejo_h = self.dir > 0
  for _, c in ipairs(self.dano:solapados()) do
    if c:en_grupo("jugador") then self:_toca(c) end
  end
end

function Limo:_toca(jugador)
  if jugador.vy > 40 and jugador.y < self.y - 4 then
    jugador:rebotar()
    Audio.sonido("audio/pisoton.wav", 0.7)
    local x, y = self:posicion_global()
    Capibara.raiz():enemigo_derrotado(x, y)
    self:liberar()
  else
    jugador:herir(self.x)
  end
end
return Limo
