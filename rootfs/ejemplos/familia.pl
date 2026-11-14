% familia.pl - ejemplo de GNU Prolog en SaguiOS (D189).
%
% Uso:  gprolog
%       | ?- ['/ejemplos/familia.pl'].
%       | ?- abuelo(X, ana).
%       | ?- hermanos(luis, Y).          (';' pide otra solucion)
%       | ?- antepasado(A, ana).
%       | ?- descendientes(pedro, L).
%       | ?- suma([3, 4, 5], S).
%       | ?- invertir([a, b, c], R).
%       | ?- halt.

% ---- hechos: progenitor(Padre_o_madre, Hijo_o_hija)
progenitor(pedro, juan).
progenitor(pedro, luis).
progenitor(maria, juan).
progenitor(maria, luis).
progenitor(juan, ana).
progenitor(juan, carlos).
progenitor(elena, ana).
progenitor(elena, carlos).
progenitor(luis, sofia).

hombre(pedro).
hombre(juan).
hombre(luis).
hombre(carlos).
mujer(maria).
mujer(elena).
mujer(ana).
mujer(sofia).

% ---- reglas
padre(P, H) :- progenitor(P, H), hombre(P).
madre(M, H) :- progenitor(M, H), mujer(M).

abuelo(A, N) :- progenitor(A, P), progenitor(P, N).

hermanos(X, Y) :- progenitor(P, X), progenitor(P, Y), X \== Y, hombre(P).

antepasado(A, D) :- progenitor(A, D).
antepasado(A, D) :- progenitor(A, X), antepasado(X, D).

descendientes(A, L) :- findall(D, antepasado(A, D), L0), sort(L0, L).

% ---- algo de listas
suma([], 0).
suma([X|Xs], S) :- suma(Xs, S0), S is S0 + X.

invertir(L, R) :- invertir(L, [], R).
invertir([], R, R).
invertir([X|Xs], A, R) :- invertir(Xs, [X|A], R).

ultimo([X], X).
ultimo([_|Xs], X) :- ultimo(Xs, X).

% contar(N, L): la lista [N, ..., 2, 1] (recursion profunda: contar(100000, L)
% llega sin problema con la pila de 16 MB de la WAM en SaguiOS).
contar(0, []) :- !.
contar(N, [N|L]) :- N1 is N - 1, contar(N1, L).
