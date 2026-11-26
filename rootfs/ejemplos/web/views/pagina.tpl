<html><head><title>{{titulo}}</title></head>
<body>
<h1>{{titulo}}</h1>
<ul>
% for it in items:
  <li>{{it}}</li>
% end
</ul>
% if len(items) > 2:
<p>Más de dos &amp; {{!"<b>sin escapar</b>"}}</p>
% else:
<p>Pocos</p>
% end
</body></html>
