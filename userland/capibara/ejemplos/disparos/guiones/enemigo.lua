-- Un avión enemigo. Variables que pone partida.lua: patron (0 recto, 1 ondas, 2 zigzag), vida, modelo (fotograma de la nave).
local Enemigo = {}

function Enemigo:_listo()
  self.t = 0
  self.x0 = self.x
  self.patron = self.patron or 0
  self.disparo = 0.8 + math.random() * 1.2
  self.dibujo = self:obtener_nodo("Dibujo")
  self.choque = self:obtener_nodo("Choque")
  if self.modelo then self.dibujo.fotograma = self.modelo end
end

function Enemigo:danar(n)
  self.vida = self.vida - n
  Audio.sonido("audio/impacto.wav", 0.35)
  if self.vida <= 0 and not self.muerto then
    self.muerto = true
    Capibara.raiz():enemigo_destruido(self.x, self.y)
    self:liberar()
  end
end

function Enemigo:_fisica(dt)
  self.t = self.t + dt
  self.y = self.y + 55 * dt
  if self.patron == 1 then self.x = self.x0 + math.sin(self.t * 2.2) * 40
  elseif self.patron == 2 then self.x = self.x0 + ((math.floor(self.t * 1.5) % 2 == 0) and 1 or -1) * 28 * math.min(1, (self.t * 1.5) % 1 * 3) end
  if self.y > 345 then self:liberar(); return end
  self.disparo = self.disparo - dt
  if self.disparo <= 0 and self.y > 10 and self.y < 220 then
    self.disparo = 1.4 + math.random() * 1.6
    Capibara.raiz():disparo_enemigo(self.x, self.y + 10)
  end
  for _, c in ipairs(self.choque:solapados()) do
    if c:en_grupo("jugador") then c:herir(); self:danar(99) end
  end
end
return Enemigo
