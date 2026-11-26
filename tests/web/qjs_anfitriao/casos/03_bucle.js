// El bucle de eventos: microtareas antes de la tarea siguiente, orden de los temporizadores,
// setInterval que se reprograma, requestAnimationFrame y MutationObserver.
var log = [];
setTimeout(function () { log.push('t20'); }, 20);
setTimeout(function () { log.push('t0a'); Promise.resolve().then(function () { log.push('micro-de-t0a'); }); }, 0);
setTimeout(function () { log.push('t0b'); }, 0);
Promise.resolve().then(function () { log.push('micro-script'); });
queueMicrotask(function () { log.push('qm'); });
var n = 0, iv = setInterval(function () { n++; if (n === 3) { clearInterval(iv); log.push('iv3'); } }, 5);
var cancelado = requestAnimationFrame(function () { log.push('NO'); });
cancelAnimationFrame(cancelado);
requestAnimationFrame(function (t) { log.push('raf'); requestAnimationFrame(function (t2) { log.push('raf2 ' + (t2 >= t)); }); });
var mo = new MutationObserver(function (r) { log.push('mo ' + r.length + ' ' + r.map(function (x) { return x.type; }).join('+')); });
mo.observe(document.getElementById('lista'), { childList: true, attributes: true, subtree: true, characterData: true });
document.getElementById('lista').appendChild(document.createElement('li'));
document.getElementById('lista').setAttribute('data-x', '1');
document.getElementById('lista').firstElementChild.firstChild.data = 'UNO';
setTimeout(function () { console.log('bucle ' + log.join(',')); }, 120);
var t0 = Date.now();
setTimeout(function () { console.log('setTimeout espera ' + (Date.now() - t0 >= 50)); }, 50);
//= setTimeout espera true
//= bucle micro-script,qm,mo 3 childList+attributes+characterData,t0a,micro-de-t0a,t0b
//= arvore-ok
