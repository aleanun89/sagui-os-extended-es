-- pruebas.lua - recorre lo principal de Lua 5.4 en SaguiOS (D440). Uso: lua pruebas.lua
-- Cada línea que imprime es comprobable; al final dice PRUEBAS_LUA_OK si no falló ninguna.
local fallos = 0
local function igual(nombre, obtenido, esperado)
    if obtenido ~= esperado then
        fallos = fallos + 1
        print("FALLO " .. nombre .. ": obtenido " .. tostring(obtenido) .. ", esperado " .. tostring(esperado))
    end
end

print(_VERSION)
-- cadenas
local s = "Hola Mundo"
print(s:upper(), s:lower(), #s, s:sub(1, 4), s:rep(2, "-"), s:reverse())
print(string.format("%5.2f|%d|%s|%q|%x|%5s|%-5s|", 3.14159, 42, "x", "a\nb", 255, "ab", "cd"))
print(("a,b,c"):find(",", 1, true), ("hola mundo"):gsub("o", "0"))
for k, v in string.gmatch("a=1, b=2", "(%w+)=(%w+)") do io.write(k, ":", v, " ") end print()
igual("match", ("2026-11-21"):match("(%d+)-(%d+)-(%d+)"), "2026")
-- tablas
local t = {5, 3, 8, 1}
table.sort(t)
print(table.concat(t, ","), #t, select("#", table.unpack(t)))
table.insert(t, 10) table.remove(t, 1)
print(table.concat(t, " "))
local claves = {}
for k in pairs({x = 1, y = 2, z = 3}) do claves[#claves + 1] = k end
table.sort(claves)
print(table.concat(claves))
-- metatablas
local V = {}
V.__index = V
V.__add = function(a, b) return setmetatable({x = a.x + b.x}, V) end
V.__tostring = function(a) return "V(" .. a.x .. ")" end
V.__eq = function(a, b) return a.x == b.x end
V.__call = function(self, n) return self.x * n end
local a, b = setmetatable({x = 1}, V), setmetatable({x = 2}, V)
print(tostring(a + b), a == b, (a + b)(10))
-- corrutinas
local co = coroutine.create(function(p)
    local q = coroutine.yield(p + 1)
    local r = coroutine.yield(q * 2)
    return "fin", r
end)
print(coroutine.resume(co, 1))
print(coroutine.resume(co, 10))
print(coroutine.resume(co, "z"))
print(coroutine.status(co), coroutine.resume(co))
local gen = coroutine.wrap(function() for i = 1, 3 do coroutine.yield(i * i) end end)
print(gen(), gen(), gen())
-- math (punto flotante y enteros de 64 bits)
print(math.pi, math.sqrt(2), math.floor(-3.5), math.ceil(3.2), math.max(1, 9, 3), math.abs(-4))
print(7 // 2, 7 % 3, 7 / 2, 2 ^ 10, 1 << 40, math.maxinteger, math.mininteger, math.type(1), math.type(1.0))
print(math.tointeger(3.0), 10 // 0.0, -10 % 3, math.fmod(-10, 3), math.sin(0), math.cos(0), math.exp(0), math.log(8, 2))
math.randomseed(42)
local r = math.random(1, 100)
igual("random", r >= 1 and r <= 100, true)
print(string.format("%.14g", 0.1 + 0.2), 1e15, 2^53, 100 // 8, 3 | 4, ~0)
-- pcall y error
print(pcall(function() error("malo") end))
local okt, et = pcall(function() error({codigo = 7}) end)
print(okt, type(et), et.codigo)
print(select(2, pcall(function() local x = nil; return x.y end)))
print(xpcall(function() error("uno") end, function(m) return "manejado: " .. m end))
igual("tonumber", tonumber("0x10"), 16)
igual("tonumber2", tonumber("  12  "), 12)
-- os
local t0 = os.time()
igual("time", t0 > 1700000000, true)
print(os.date("!%Y-%m-%d %H:%M:%S", 86400 * 365))
print(os.time({year = 2026, month = 11, day = 21, hour = 12}) - os.time({year = 2026, month = 11, day = 20, hour = 12}))
igual("clock", type(os.clock()), "number")
print(os.getenv("LUAPRUEBA"), os.getenv("NOEXISTE_XYZ"))
print(os.execute("exit 3"))
print(os.execute("true"))
-- io
local nombre = os.tmpname()
local f = assert(io.open(nombre, "w"))
f:write("linea uno\n", "linea dos\n", 42, " ", 3.5, "\n")
f:close()
for l in io.lines(nombre) do io.write("[", l, "]") end print()
f = io.open(nombre, "r")
print(f:read("l"), f:read("n"), f:read("a"))
print(f:seek("set", 0), f:read(5), f:seek("cur"), f:seek("end"))
f:close()
f = io.open(nombre, "a") f:write("final") f:close()
f = io.open(nombre) local todo = f:read("a") f:close()
print(#todo, select(2, todo:gsub("\n", "")))
os.remove(nombre)
local fo, _, fcod = io.open("/no/existe/nada", "r")   -- el texto del error depende del idioma de la libc
print(fo, fcod)
local p = io.popen("echo desde popen; echo segunda", "r")
print(p:read("a"))
print(p:close())
p = io.popen("cat", "w") p:write("escrito al pipe\n") p:close()
-- require de un módulo propio (package.path incluye ./?.lua)
local u = require("miutil")
print(u.nombre, u.doble(21), u.saludo("SaguiOS"), package.loaded.miutil == u)
print(pcall(require, "no_existe_modulo") == false)
-- goto, enteros/flotantes, cierres
do
    local i = 0
    ::otra::
    i = i + 1
    if i < 3 then goto otra end
    igual("goto", i, 3)
end
local function contador() local c = 0 return function() c = c + 1 return c end end
local c1, c2 = contador(), contador()
c1() c1()
print(c1(), c2())
print(utf8.char(72, 228, 8364), utf8.len("häh"), #utf8.char(8364))
print(string.pack("<i4", 1):byte(1, -1))
print(load("return 1 + 1")(), load("syntax error here"))
print(collectgarbage("count") > 0, collectgarbage("collect"))
if fallos == 0 then print("PRUEBAS_LUA_OK") else print("FALLOS", fallos) end
