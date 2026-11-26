-- valkey.lua - cliente pequeño de Valkey para Lua de SaguiOS (D477). Habla RESP2 por el módulo «socket»
-- (conectar, send, receive, close) que trae el Lua de SaguiOS.
--
--   local valkey = require("valkey")
--   local r = valkey.conectar("127.0.0.1", 6379)
--   r:set("a", 1); print(r:get("a"))           --> 1
--   r:rpush("l", "x", "y"); print(table.concat(r:lrange("l", 0, -1), ","))
--   r:comando("ZADD", "z", 1, "uno")            -- cualquier orden
local socket = require("socket")

local M = {}
local Cliente = {}
Cliente.__index = Cliente

function M.conectar(host, puerto)
  local c, err = socket.connect(host or "127.0.0.1", puerto or 6379)
  if not c then return nil, err end
  return setmetatable({ c = c }, Cliente)
end

function Cliente:cerrar() self.c:close() end

local function enviar(self, args)
  local p = { "*" .. #args .. "\r\n" }
  for _, a in ipairs(args) do
    a = tostring(a)
    p[#p + 1] = "$" .. #a .. "\r\n" .. a .. "\r\n"
  end
  assert(self.c:send(table.concat(p)))
end

local function leer(self)
  local l = assert(self.c:receive("l"))
  local t, resto = l:sub(1, 1), l:sub(2)
  if t == "+" then return resto
  elseif t == "-" then error("valkey: " .. resto, 0)
  elseif t == ":" then return math.tointeger(tonumber(resto))
  elseif t == "$" then
    local n = tonumber(resto)
    if n < 0 then return nil end
    local d = assert(self.c:receive(n + 2))
    return d:sub(1, n)
  elseif t == "*" then
    local n = tonumber(resto)
    if n < 0 then return nil end
    local r = {}
    for i = 1, n do r[i] = leer(self) end
    return r
  end
  error("respuesta rara: " .. l)
end

function Cliente:comando(...)
  enviar(self, { ... })
  return leer(self)
end

function Cliente:leer() return leer(self) end
function Cliente:enviar(...) enviar(self, { ... }) end

for _, o in ipairs({ "ping", "set", "get", "incr", "incrby", "expire", "ttl", "exists", "keys", "lpush", "rpush",
                     "lrange", "hset", "hget", "publish", "del" }) do
  Cliente[o] = function(self, ...) return self:comando(o:upper(), ...) end
end
function Cliente:hgetall(k)
  local l = self:comando("HGETALL", k)
  local t = {}
  for i = 1, #l, 2 do t[l[i]] = l[i + 1] end
  return t
end

return M
