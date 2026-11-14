// Ejemplo para qjs: qjs /ejemplos/hola.js
console.log("Hola desde QuickJS en SaguiOS!");
const fib = n => (n < 2 ? n : fib(n - 1) + fib(n - 2));
console.log("fib(20) =", fib(20));
console.log({ sistema: os.platform, args: scriptArgs });
