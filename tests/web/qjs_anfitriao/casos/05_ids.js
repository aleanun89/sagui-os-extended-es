// getElementById con el índice: quitar, volver a meter, cambiar el id, ids repetidos e innerHTML.
var t = document.getElementById('t');
t.remove();
console.log('quitado ' + document.getElementById('t'));
document.body.appendChild(t);
console.log('vuelto ' + (document.getElementById('t') === t));
t.id = 'otro';
console.log('cambiado ' + document.getElementById('t') + ' ' + (document.getElementById('otro') === t));
var a = document.createElement('p'); a.id = 'rep'; document.body.appendChild(a);
var b = document.createElement('p'); b.id = 'rep'; document.body.appendChild(b);
console.log('repetido ' + (document.getElementById('rep') === a));
a.remove();
console.log('repetido2 ' + (document.getElementById('rep') === b));
document.getElementById('caja').innerHTML = '<i id="dentro">x</i>';
console.log('innerHTML ' + document.getElementById('dentro').tagName + ' ' + document.getElementById('s'));
var c = document.createElement('div'); c.innerHTML = '<span id="suelto"></span>';
console.log('suelto ' + document.getElementById('suelto'));
document.body.appendChild(c);
console.log('metido ' + document.getElementById('suelto').tagName);
var k = document.getElementById('suelto').cloneNode(); k.id = 'clon';
console.log('clon ' + document.getElementById('clon'));
document.body.appendChild(k);
console.log('clon2 ' + document.getElementById('clon').tagName);
//= quitado null
//= vuelto true
//= cambiado null true
//= repetido true
//= repetido2 true
//= innerHTML I null
//= suelto null
//= metido SPAN
//= clon null
//= clon2 SPAN
//= arvore-ok
