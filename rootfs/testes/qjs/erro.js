function interna() { throw new TypeError("fallo adrede"); }
function externa() { interna(); }
console.log("antes");
externa();
console.log("nunca");
