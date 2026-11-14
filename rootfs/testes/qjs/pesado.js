// h155: cálculo algo pesado y recursión profunda
function primos(n) {
  const c = new Uint8Array(n + 1); let k = 0;
  for (let i = 2; i <= n; i++) {
    if (!c[i]) { k++; for (let j = i * 2; j <= n; j += i) c[j] = 1; }
  }
  return k;
}
function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
function prof(n) { return n === 0 ? 0 : 1 + prof(n - 1); }
const t0 = os.now();
console.log("primos", primos(2000000));
console.log("fib", fib(25));
console.log("prof", prof(20000));
let grande = []; for (let i = 0; i < 200000; i++) grande.push({ i, s: "x" + i });
console.log("memoria", grande.length, grande[199999].s);
let overflow = false;
try { (function r() { r(); })(); } catch (e) { overflow = e instanceof InternalError || e instanceof RangeError; }
console.log("pila", overflow);
console.log("tiempo_ok", os.now() - t0 >= 0);
