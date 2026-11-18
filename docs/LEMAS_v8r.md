# Lemas nuevos de v8-r y del prototipo OOO

Notación del libro: `≺` es el comparador estricto; Mrg(X, Y) es la mezcla estable (en empates va
primero X). Estado: los tres lemas están escritos a mano; ninguno está mecanizado.

## Lema S (sonda de rachas)

Sean X = A[l..m) e Y = A[m..r) ordenados, con a = |X| y b = |Y|. La sonda toma
px_0 = l, px_i = l + ⌊i·a/9⌋ para i = 1..8, px_9 = m, y
py_0 = m, py_i = primer índice de [py_{i-1}, r) con A[py_i] ⊀ A[px_i] (i = 1..8), py_9 = r.
Define sw = Σ_{i=1..9} (2·min(px_i − px_{i−1}, py_i − py_{i−1}) + 1).

**(a)** La cantidad de cambios de lado de Mrg(X, Y) es a lo sumo sw.

*Demostración.* Para i = 1..8, todo y de Y antes de py_i cumple y ≺ A[px_i], así que precede a
A[px_i] en la mezcla; todo y desde py_i cumple y ⊀ A[px_i] y, como en empates va primero X, queda
después de A[px_i]. Entonces Mrg(X, Y) es la concatenación, para i = 1..9, de
Mrg(A[px_{i−1}..px_i), A[py_{i−1}..py_i)). Una mezcla de gx y gy elementos tiene a lo sumo
2·min(gx, gy) cambios de lado, y entre dos tramos consecutivos hay a lo sumo uno más. ∎

**(b)** La sonda no afecta la corrección ni la estabilidad: solo elige entre dos procedimientos
que ya cumplen la misma poscondición (parity merge de largos distintos y Merge-Izq/Der con galope).

**(c)** Costo: a lo sumo 8⌈lg(b+1)⌉ comparaciones por mezcla, y solo si a + b ≥ 256. En el camino
de runs naturales hay a lo sumo ⌈n/24⌉ − 1 mezclas, así que el total es O(n log n) en el peor
caso. Con mezclas balanceadas de tamaño s ≥ 256 hay n/s por nivel: Σ (n/s)·8 lg s = O(n).

Regla usada: galope si sw·8 ≤ a + b (rachas de largo medio ≥ 8 garantizadas); si no, parity merge.
El umbral 8 y las 8 muestras se eligieron midiendo en una sola máquina.

## Lema B (buffers disjuntos en modo Completo con `follow_buffer`)

Con `follow_buffer(aux)`, mientras QuickEstable procesa el rango [lo, hi) vale buf = aux + lo y
B = hi − lo. Toda escritura en el buffer cae en aux[lo..hi).

*Demostración (por casos sobre lo que se ejecuta con ese rango).*
- Partición-Estable: como hi − lo ≤ B hay un solo bloque. La versión escalar escribe buf[i − m]
  con i < hi − lo. La vectorial escribe buf[i − m .. i − m + w) con i + w ≤ hi − lo.
- Hoja (hi − lo ≤ L): los trozos escriben buf[0..c) y el parity merge buf[0..tot), con
  c ≤ tot ≤ hi − lo.
- Pivote: s ≤ B muestras.
- Respaldo: SymMerge sobre subrangos [l', r') de [lo, hi) escribe a lo sumo min(nl, nr) o
  nl + nr ≤ r' − l' celdas desde buf[0].
Rotar no se ejecuta (un solo bloque: no hay fusiones). ∎

*Corolario.* La huella de una tarea sobre [lo, hi) está contenida en A[lo..hi) ∪ aux[lo..hi). Dos
tareas vivas tienen rangos disjuntos (invariante de QuickEstable), así que sus huellas son
disjuntas y, por las condiciones de Bernstein, conmutan.

Los rangos de la pila local de un trabajador también son disjuntos de todo rango cedido, porque
una cesión entrega una de las dos mitades de una partición y el trabajador sigue con la otra.

## Lema R (partición paralela de la raíz)

Sean d_0 = 0 < d_1 < … < d_P = n, y P(x) ≡ x ≺ p. Fase 1: para cada tramo i, los que cumplen P
quedan en A[d_i .. d_i + m_i) y el resto en aux[d_i .. d_i + f_i), ambos en su orden original
(Partición-Bloque sin la copia final). Fase 2: para i = 1..P−1, en orden, se mueve
A[d_i .. d_i + m_i) a A[M_i ..), con M_i = Σ_{j<i} m_j. Fase 3: aux[d_i .. d_i + f_i) se copia a
A[M + F_i ..), con M = Σ m_j y F_i = Σ_{j<i} f_j.

Entonces A = filtro(P, A_0) ++ filtro(¬P, A_0), y es coherente.

*Demostración.* Fase 1: cada tramo usa A[d_i..d_{i+1}) y aux[d_i..d_{i+1}), disjuntos entre
tramos. Fase 2: M_i ≤ d_i, así que el destino empieza a la izquierda del origen y un movimiento
hacia adelante es seguro; el destino [M_i, M_i + m_i) no pisa orígenes de tramos posteriores
porque M_i + m_i ≤ d_i + m_i ≤ d_{i+1}. Al terminar, A[0..M) es la concatenación en orden de tramos
de los que cumplen P. Fase 3: los destinos [M + F_i, M + F_i + f_i) son disjuntos entre sí, están
en A[M..n), y los orígenes están en aux; A[M..n) ya no contiene nada pendiente. El orden relativo
dentro de cada clase es el original porque cada fase conserva el orden dentro del tramo y los
tramos se concatenan en orden. ∎

Costo: W = n evaluaciones de P; movimientos ≤ 2n (igual que la versión secuencial) más los M − m_0
de la fase 2. Camino crítico: n/P + (M − m_0) + máx f_i.
