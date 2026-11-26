// El árbol: consultas, appendChild, insertBefore, removeChild, innerHTML, plantillas y clones.
console.log('h1 ' + document.getElementById('t').textContent);
console.log('lis ' + document.querySelectorAll('#lista > li').length + ' ' + document.querySelector('li.b.x').textContent);
var ul = document.getElementById('lista');
for (var i = 0; i < 3; i++) { var li = document.createElement('li'); li.textContent = 'n' + i; li.className = 'n'; ul.appendChild(li); }
console.log('ahora ' + ul.children.length + ' ' + ul.lastElementChild.textContent + ' ' + document.querySelectorAll('li:nth-child(odd)').length);
ul.insertBefore(document.createElement('li'), ul.firstChild);
ul.removeChild(ul.children[1]);
console.log('html ' + ul.innerHTML);
ul.innerHTML = '<li id="z">zeta <b>negrita</b></li><li>otra</li>';
console.log('tras innerHTML ' + ul.children.length + ' ' + document.getElementById('z').querySelector('b').textContent);
var f = document.createDocumentFragment();
f.append('texto', document.createElement('i'));
ul.lastElementChild.appendChild(f);
console.log('fragmento ' + ul.lastElementChild.innerHTML + ' ' + f.childNodes.length);
var c = ul.cloneNode(true);
c.id = 'copia';
document.body.appendChild(c);
console.log('clon ' + c.children.length + ' ' + (c.querySelector('#z') !== document.getElementById('z')));
ul.firstElementChild.replaceWith(document.createElement('hr'));
console.log('replaceWith ' + ul.firstChild.tagName);
ul.insertAdjacentHTML('beforeend', '<li class="adj">adyacente</li>');
console.log('adyacente ' + ul.querySelector('.adj').textContent + ' ' + ul.childElementCount);
document.getElementById('t').outerHTML = '<h2 id="t2">nuevo</h2>';
console.log('outerHTML ' + document.getElementById('t2').tagName + ' ' + (document.getElementById('t') === null));
var cl = document.body.classList; cl.add('a', 'b'); cl.toggle('a'); cl.replace('b', 'c');
console.log('classList ' + document.body.className + ' ' + cl.contains('c'));
var d = document.createElement('div'); d.style.backgroundColor = 'red'; d.style.width = 10; d.dataset.miValor = '7';
console.log('style ' + d.getAttribute('style') + ' ' + d.getAttribute('data-mi-valor') + ' ' + d.style.backgroundColor);
//= h1 Hola
//= lis 2 dos
//= ahora 5 n2 3
//= html <li></li><li class="b x">dos</li><li class="n">n0</li><li class="n">n1</li><li class="n">n2</li>
//= tras innerHTML 2 negrita
//= fragmento otratexto<i></i> 0
//= clon 2 true
//= replaceWith HR
//= adyacente adyacente 3
//= outerHTML H2 true
//= classList c true
//= style background-color: red; width: 10px; 7 red
//= arvore-ok
