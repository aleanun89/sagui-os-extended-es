import Punto, { nombre, doble } from "./mod.mjs";
import * as os2 from "os";
import { loadFile } from "std";
console.log("modulo", nombre, doble(21), new Punto(5).x, typeof os2.sleep, typeof loadFile);
const r = await Promise.resolve(7);
console.log("tla", r);
