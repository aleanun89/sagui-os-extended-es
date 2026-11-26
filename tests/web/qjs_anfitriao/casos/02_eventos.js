// Eventos: captura, burbuja, once, stopPropagation, preventDefault, teclado, campos, envío y foco.
var orden = [];
var caja = document.getElementById('caja'), s = document.getElementById('s');
document.addEventListener('click', function () { orden.push('doc-cap'); }, true);
document.addEventListener('click', function () { orden.push('doc-burb'); });
caja.addEventListener('click', function () { orden.push('caja-cap'); }, true);
caja.addEventListener('click', function () { orden.push('caja-burb'); });
s.addEventListener('click', function (e) { orden.push('span' + e.eventPhase); });
s.addEventListener('click', function () { orden.push('once'); }, { once: true });
window.addEventListener('click', function () { console.log('orden ' + orden.join(',')); orden = []; });
var parar = function (e) { if (e.detail === 2) e.stopPropagation(); };
s.addEventListener('dblclick', function (e) { console.log('dblclick ' + e.detail); });
document.getElementById('f').addEventListener('submit', function (e) { e.preventDefault(); console.log('submit ' + document.getElementById('campo').value); });
document.getElementById('campo').addEventListener('input', function () { console.log('input ' + this.value); });
document.getElementById('campo').addEventListener('focus', function () { console.log('foco campo'); });
document.getElementById('campo').addEventListener('change', function () { console.log('change ' + this.value); });
document.addEventListener('keydown', function (e) { console.log('tecla ' + e.key + ' ' + e.keyCode + ' ' + e.code); if (e.key === 'ArrowUp') e.preventDefault(); });
document.addEventListener('mouseover', function (e) { console.log('over ' + e.target.id); });
caja.addEventListener('mouseenter', function () { console.log('enter caja'); });
caja.addEventListener('mouseleave', function () { console.log('leave caja'); });
var ce = new CustomEvent('mio', { detail: { v: 42 }, bubbles: true });
document.body.addEventListener('mio', function (e) { console.log('custom ' + e.detail.v + ' ' + (e.target === s)); });
s.dispatchEvent(ce);
s.addEventListener('quieto', function (e) { e.stopImmediatePropagation(); console.log('primero'); });
s.addEventListener('quieto', function () { console.log('NO DEBE'); });
s.dispatchEvent(new Event('quieto'));
//! click #s
//! click #s
//! dblclick #s {"detalhe":2}
//! mousemove #s {"x":5,"y":5}
//! mousemove #t {"x":5,"y":5}
//! mousedown #campo
//! input #campo {"valor":"hola"}
//! submit #campo
//! keydown -1 {"tecla":"ArrowUp"}
//! keydown -1 {"tecla":"a"}
//! mousedown #t
//= custom 42 true
//= primero
//= orden doc-cap,caja-cap,span2,once,caja-burb,doc-burb
//= orden doc-cap,caja-cap,span2,caja-burb,doc-burb
//= dblclick 2
//= over s
//= enter caja
//= leave caja
//= over t
//= foco campo
//= input hola
//= submit hola
//= [evento submit
//= tecla ArrowUp 38 ArrowUp
//= [evento keydown -1] cancelado
//= tecla a 65 KeyA
//= change hola
//= arvore-ok
