-- miutil.lua - un módulo propio para probar require (D440)
local M = {}
function M.doble(x) return x * 2 end
function M.saludo(n) return "hola, " .. n end
M.nombre = "miutil"
return M
