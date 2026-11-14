// h155: aritmética, cadenas, JSON, Map/Set, clases, async, regex, ficheros
console.log("aritm", 2 ** 10 + 7 % 4, 0.1 + 0.2);
console.log("cadena", "sagui".toUpperCase() + "-" + "x".repeat(3), "ñandú".length);
console.log("json", JSON.stringify({ a: [1, 2, { b: null }], c: "d" }));
console.log("json2", JSON.parse('{"x":[1,2,3]}').x.length);
const m = new Map([["a", 1], ["b", 2]]);
const s = new Set([1, 2, 2, 3]);
console.log("mapset", m.get("b"), s.size, [...s].join(","));
class Animal { constructor(n) { this.n = n; } hablar() { return this.n + " hace ruido"; } }
class Perro extends Animal { hablar() { return super.hablar() + " (guau)"; } }
console.log("clase", new Perro("Tito").hablar());
console.log("regex", "tel 555-1234 y 555-9876".match(/\d{3}-\d{4}/g).join("|"), /(\w+)@(\w+)/.exec("ana@sagui")[2]);
console.log("args", scriptArgs.length, scriptArgs.slice(1).join(","));
std.writeFile("/tmp/qjs_prueba.txt", "linea uno\nlínea dos\n");
std.appendFile("/tmp/qjs_prueba.txt", "tres\n");
const t = std.loadFile("/tmp/qjs_prueba.txt");
console.log("fichero", t.length, t.split("\n").length - 1, std.loadFile("/no/existe"));
console.log("env", typeof std.getenv("PATH"), std.getenv("NO_EXISTE_XYZ"));
console.log("fecha", new Date(0).toISOString(), Date.now() > 1.6e12);
const antes = os.now(); os.sleep(120); console.log("sleep", os.now() - antes >= 100);
console.log("dir", os.readdir("/tmp").includes("qjs_prueba.txt"), os.stat("/tmp/qjs_prueba.txt").size);
const orden = [];
setTimeout(() => orden.push("timer"), 30);
Promise.resolve().then(() => orden.push("micro"));
(async () => {
  await null; orden.push("await");
  await new Promise(r => setTimeout(r, 60));
  orden.push("tarde");
  console.log("async", orden.join(">"));
})();
orden.push("sync");
process_fin = () => console.log("fin script");
process_fin();
