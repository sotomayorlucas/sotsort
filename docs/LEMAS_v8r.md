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

Regla usada: galope si sw·8 ≤ a + b (rachas de largo medio ≥ 8 garantizadas); si no, parity merge
cuando a + b ≤ B, y el lazo clásico con saltos cuando no entran juntos. Solo tipos triviales.
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

## Lema Z (zonas desordenadas) y regla de ruta

Enunciados y demostraciones completos: secciones 13.4 y 13.5 de `sotsort_demo.pdf`
(Observación 13.3 y Lema 13.4). Resumen:
- **Regla de ruta:** solo elige entre MergeNatural y QuickEstable, ambos correctos para toda
  entrada. Cuesta 255 comparaciones y no escribe.
- **Zonas:** la zona [i, e) es un intervalo todavía no apilado y coherente; QuickEstable la deja
  ordenada y coherente, que es lo que el invariante de MergeNatural pide de un run. El recorrido
  hacia adelante agrega a lo sumo 3n comparaciones.

## Observación T (v8-r3: giros estimados por muestreo)

Con n ≥ 2^16, `few_turns` ya no cuenta todos los giros: estima su cantidad con 64 ventanas de 256
pares consecutivos, una por tramo de n/64 elementos con desplazamiento pseudoaleatorio, y escala
el conteo a n. Con n < 2^16, o compilando con `-DSOT_EXACT_TURNS`, el conteo sigue siendo exacto.

- **Corrección:** la estimación solo alimenta la regla de ruta, que elige entre MergeNatural y
  QuickEstable; los dos son correctos para toda entrada (Observación 13.3). Un error de estimación
  cambia el camino, no el resultado. No escribe en el arreglo.
- **Costo:** 64·256 = 16384 comparaciones fijas, contra n cuando hay pocos giros (antes el conteo
  exacto no podía cortar antes de recorrer todo el arreglo). La detección de runs de MergeNatural
  sigue recorriendo el arreglo una vez; el recorrido de los giros era el que sobraba.
- **Garantía de peor caso:** sin cambios. Ninguno de los dos caminos depende de la decisión para
  su cota O(n log n).

## Observación P (v8-r6: tope de particiones malas)

El presupuesto de particiones desparejas de QuickEstable pasa de ⌊lg n⌋ + 1 a
min(⌊lg n⌋ + 1, 4) (`SOT_BAD_BUDGET`). Cuando se agota, el rango termina en la mezcla de
respaldo, igual que antes.

- **Corrección:** el presupuesto solo decide cuándo se usa el respaldo; el respaldo es un orden
  estable correcto para cualquier entrada. Sin cambios en los invariantes de la partición.
- **Terminación:** el presupuesto sigue siendo finito y decrece en cada paso desparejo (incluido
  el de separar iguales, v6.1); la recursión sobre el lado chico no cambia.
- **Costo:** peor caso O(n log n) igual que antes, con el respaldo alcanzado después de a lo sumo
  4 pasos desparejos por camino. Medido: el adversario de McIlroy baja de 95.6 a 29.3 movimientos
  por elemento (n = 10^6, presupuesto de VM-Powersort); en nueve distribuciones de `bench/` los
  conteos no cambian (`bench/cost/bad_budget_dists.cpp`).

## Observación S (v8-r6: samplesort estable por páginas)

`pss_sort` reemplaza a QuickEstable en la raíz cuando hay buffer de bloques (B < n/2) y la ruta
eligió el quicksort. Demostración de la pasada (lemas 1'–3': siempre hay ranura libre al volcar;
el índice de ranura es el orden de volcado; el espacio final mide la suma de las parciales),
fases y costo: `docs/IDEA_particion_paginada.md` §2b, §2d, §2f, §2i.

- **Corrección:** cada pasada es una distribución estable por grupo (grupo = cantidad de
  separadores ≤ x, así que los iguales caen juntos y los grupos quedan ordenados entre sí); los
  casos base son QuickEstable, ya demostrado. Verificación exhaustiva de la pasada para claves en
  {0..k−1}, k = 2..4, P = 1..3, m ≤ 9.
- **Terminación:** cada pasada sin grupo de tamaño m achica todos los rangos; con un grupo de
  tamaño m se va al caso base; la profundidad está acotada por 2·log_k n + 2 y por dos pasadas
  malas seguidas.
- **Memoria:** páginas y separadores dentro del buffer; tablas (6 bytes por ranura) dentro de la
  reserva de tablas del plan, verificada por `tests/test_mem.cpp`.
