-- Un murciélago: vuela en una onda alrededor de su sitio, sin colisionar con el mapa.
local Murcielago = {}
function Murcielago:_listo()
  self.x0, self.y0 = self.x, self.y
  self.t = 0
  self.dano = self:obtener_nodo("Dano")
end
function Murcielago:_fisica(dt)
  self.t = self.t + dt
  self.x = self.x0 + math.sin(self.t * 1.3) * 48
  self.y = self.y0 + math.sin(self.t * 2.6) * 14
  for _, c in ipairs(self.dano:solapados()) do
    if c:en_grupo("jugador") then
      if c.vy > 40 and c.y < self.y - 3 then
        c:rebotar(); Audio.sonido("audio/pisoton.wav", 0.7)
        Capibara.raiz():enemigo_derrotado(self.x, self.y)
        self:liberar()
      else c:herir(self.x) end
    end
  end
end
return Murcielago
