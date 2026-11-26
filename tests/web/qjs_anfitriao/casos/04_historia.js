// location, history.pushState/replaceState/popstate y hashchange; medidas.
console.log('loc ' + location.href + ' ' + location.pathname + ' ' + location.host + ' ' + location.origin);
history.pushState({ p: 2 }, '', 'pagina2?x=1');
console.log('push ' + location.pathname + location.search + ' ' + history.state.p + ' ' + history.length);
history.replaceState({ p: 3 }, '', '/otra');
console.log('replace ' + location.pathname + ' ' + history.state.p + ' ' + history.length);
window.addEventListener('popstate', function (e) { console.log('popstate ' + JSON.stringify(e.state) + ' ' + location.pathname); });
window.addEventListener('hashchange', function (e) { console.log('hashchange ' + location.hash); });
location.hash = 't';
try { history.pushState(null, '', 'http://otro.sitio/x'); } catch (e) { console.log('origen ' + e.name); }
var r = document.getElementById('lista').getBoundingClientRect();
console.log('rect ' + (typeof r.top) + ' ' + r.width + ' ' + document.getElementById('lista').offsetWidth + ' ' + innerWidth + ' ' + innerHeight);
console.log('computed ' + getComputedStyle(document.body).display);
console.log('desde punto ' + (document.elementFromPoint(1, 1) !== null));
scrollTo(0, 25);
console.log('scrollY ' + scrollY + ' ' + document.documentElement.scrollTop);
//! tique 30
//! popstate -1 {"delta":-1}
//= loc http://exemplo.local/app/index.html /app/index.html exemplo.local http://exemplo.local
//= [historia] push http://exemplo.local/app/pagina2?x=1
//= push /app/pagina2?x=1 2 2
//= [historia] replace http://exemplo.local/otra
//= replace /otra 3 2
//= [historia] push http://exemplo.local/otra#t
//= origen SecurityError
//= rect number 100 100 800 600
//= computed block
//= desde punto true
//= [rolar] 0 25
//= scrollY 25 25
//= hashchange #t
//= popstate {"p":3} /otra
//= arvore-ok
