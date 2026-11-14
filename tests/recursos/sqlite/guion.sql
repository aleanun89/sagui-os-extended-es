CREATE TABLE clientes(id INTEGER PRIMARY KEY, nombre TEXT NOT NULL, ciudad TEXT, saldo REAL);
CREATE TABLE pedidos(id INTEGER PRIMARY KEY, cliente INTEGER REFERENCES clientes(id), total REAL, nota TEXT);
CREATE INDEX idx_ped_cli ON pedidos(cliente);
CREATE VIEW grandes AS SELECT id, nombre FROM clientes WHERE saldo > 600;
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<500)
  INSERT INTO clientes SELECT i, 'cliente'||i, CASE i%3 WHEN 0 THEN 'Madrid' WHEN 1 THEN 'Sevilla' ELSE 'Lima' END, i*1.25 FROM n;
WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<2000)
  INSERT INTO pedidos SELECT i, (i*7)%500+1, (i%97)*0.5+1, 'pedido '||i||' con ñandú' FROM n;
SELECT ciudad, count(*), round(sum(saldo),2) FROM clientes GROUP BY ciudad ORDER BY ciudad;
SELECT c.nombre, round(sum(p.total),2) AS t FROM clientes c JOIN pedidos p ON p.cliente=c.id GROUP BY c.id ORDER BY t DESC, c.nombre LIMIT 5;
SELECT count(*) FROM grandes;
.headers on
.mode csv
SELECT id, nombre, ciudad FROM clientes WHERE id IN (3, 250, 499) ORDER BY id;
SELECT 'a,b', 'dice "hola"', NULL, 1.5, x'0102' ;
.headers off
.mode list
BEGIN;
DELETE FROM pedidos;
SELECT count(*) FROM pedidos;
ROLLBACK;
SELECT count(*) FROM pedidos;
UPDATE clientes SET saldo = saldo + 100 WHERE ciudad = 'Lima';
SELECT round(sum(saldo),2) FROM clientes;
DELETE FROM pedidos WHERE id % 2 = 0;
PRAGMA integrity_check;
VACUUM;
PRAGMA integrity_check;
SELECT count(*), min(id), max(id) FROM pedidos;
SELECT typeof(saldo), typeof(nombre), typeof(NULL), typeof(x'00'), typeof(1) FROM clientes LIMIT 1;
SELECT substr(nota, 1, 8), length(nota) FROM pedidos WHERE id = 1001;
.tables
.schema clientes
.mode column
.headers on
SELECT id, nombre, saldo FROM clientes WHERE id < 4;
