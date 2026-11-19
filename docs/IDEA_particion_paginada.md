# Partición estable paginada — nota de trabajo (19 nov 2026)

Motivo: la respuesta de Sebastian Wild ("what counts more to me are robust algorithmic
ideas"). El objetivo deja de ser el tiempo y pasa a ser el costo en comparaciones y
movimientos, con cotas demostradas.

## 1. Costo medido de la partición actual (v8-r5)

`stable_partition(lo, hi)` con buffer de B elementos: particiona bloques de B a través del
buffer y junta los bloques con rotaciones en orden de contador binario (lg(m/B) niveles).
Medido con un tipo que cuenta asignaciones y construcciones (`bench/cost/partition_moves.cpp`, camino
genérico, pivote = mediana, una sola llamada):

| m | B | m/B | movimientos / elemento |
|---|---|---|---|
| 2^16 | 65536 | 1 | 1.50 |
| 2^22 | 2^20 | 4 | 3.00 |
| 2^16 | 2048 | 32 | 4.69 |
| 2^20 | 2048 | 512 | 6.71 |
| 2^22 | 2048 | 2048 | 7.70 |

Comparaciones: 1.00 por elemento en todos los casos.
Ajuste empírico: movimientos ≈ 1.5 + c·lg(m/B), con c entre 0.56 y 0.75. Es decir,
**Θ(m log(m/B))** movimientos por partición. Sumado sobre los niveles del quicksort da
Θ(n log n · log(n/B)) en el peor caso de buffer chico, y explica los 49.6 movimientos por
elemento contra 30.7 de VM-Powersort en n = 10^7, S = 2.
(Pendiente: derivar c exactamente a partir de los casos de `rotate`.)

## 2. Idea: partición estable por páginas

Inspirada en el esquema de páginas de VM-Powersort (Moltmann, Nakajima, Wild, ESA 2026) y
en la permutación de bloques de IPS4o (Axtmann, Witt, Ferizovic, Sanders, ESA 2017).

1. Páginas de P elementos; el buffer guarda dos páginas de salida (una para "<", otra para
   "≥").
2. Se lee la entrada de izquierda a derecha; cada elemento va a su página de salida
   (1 movimiento). Cuando una página de salida se llena, se vuelca en la **primera ranura
   libre** del arreglo (1 movimiento): una ranura queda libre cuando se terminó de leer.
3. Afirmación a demostrar: siempre hay una ranura libre al volcar (al volcar la k-ésima
   página se leyeron al menos k·P elementos), y como se vuelca siempre en la primera libre y
   las ranuras se liberan de izquierda a derecha, **el orden físico de las ranuras es el
   orden de volcado**. Entonces alcanza con **un bit por ranura** (de qué lado es) para
   saber el orden estable de las páginas de cada lado: no hace falta tabla de índices.
4. Al final, una permutación por ciclos lleva las páginas "<" (en orden) al frente y las
   "≥" (en orden) al fondo; el destino de cada ranura sale de contar bits (prefijo).
   Cada página fuera de lugar se mueve una vez.
5. Las dos páginas parciales del buffer y el desfase de alineación (m mod P) se resuelven
   con O(P) movimientos más un corrimiento de la parte "≥" (a lo sumo 1 movimiento por
   elemento de esa parte). Diseño exacto pendiente.

Costo esperado: ≈ 2 (clasificar + volcar) + ≤ 1 (permutación) + ≤ 0.5 (corrimiento)
= **≤ 3.5 movimientos por elemento, independiente de B**; 1 comparación por elemento;
memoria extra 2P elementos + 2 bits por página.
Contra la actual (1.5 + c·lg(m/B)) gana cuando m/B ≳ 8–16, que en un quicksort con
buffer chico es casi siempre en los niveles altos.

Lo que ya se puede afirmar: con buffer chico, el quicksort pasaría de
Θ(n log n log(n/B)) a Θ(n log n) movimientos esperados. Es una cota, no un tiempo.

## 2b. Prototipo y demostración (19 nov 2026, tarde)

Prototipo: `bench/cost/paged_partition.cpp`. Algoritmo exacto (páginas de P, buffer 2P):

- **Lectura.** Para r = 0..m−1: el elemento va al final de LB o de RB (1 movimiento). Si una
  página de salida se llena, se copia a la ranura w = [wP, (w+1)P) (P movimientos), se
  anota el lado en el bit w y w ← w+1.
- **Fase 1.** LB (nl elementos) y RB (nr) se copian a [wP, m).
- **Fase 2.** Permutación por ciclos de las w páginas: destino de una izquierda = cantidad de
  izquierdas antes; de una derecha = (total de izquierdas) + derechas antes. Se sigue cada
  ciclo hacia atrás con un hueco: una página al buffer y luego cada página una sola vez.
  El origen de cada ranura se obtiene con *select* sobre el bitset (prefijos por palabra).
- **Fase 3.** [L·P | R·P | nl | nr] → [L | nl | R | nr]: la parcial izquierda se pasa por el
  buffer y las páginas derechas se corren nl lugares. La parcial derecha ya está en su lugar.

**Lema 1 (siempre hay ranura).** Al volcar con w = f, se leyeron q = fP + nl + nr elementos y
uno de los dos vale P, así que q ≥ (f+1)P. Las posiciones [0, q) ya se leyeron; las ranuras
< f tienen páginas volcadas y la ranura f no tiene elementos sin leer ni páginas volcadas.
Escribir ahí no destruye nada.

**Lema 2 (orden).** Se vuelca siempre en la ranura w y w crece de a uno, así que el índice
de ranura es el orden de volcado. Cada página de salida recibe sus elementos en orden de
lectura. Por lo tanto, los izquierdos en orden estable son: páginas izquierdas por índice de
ranura creciente y luego LB; igual del lado derecho. Un bit por ranura alcanza.

**Lema 3 (espacio final).** Al terminar, m = wP + nl + nr, así que [wP, m) mide exactamente
nl + nr, todo leído y sin volcar.

Con los tres, la fase 2 es una permutación de ranuras (biyección) y el resultado de la fase 3
es [izquierdos en orden | derechos en orden]: la partición es estable.

**Costo exacto.** Comparaciones: m (un predicado por elemento). Movimientos:
2m + (M + C)·P + b·P + 2·nl, donde M = páginas fuera de lugar en la fase 2, C = ciclos de
largo ≥ 2 y b = páginas derechas. Como C ≤ M/2, M ≤ w y b ≤ w: **a lo sumo 4.5m + O(P)**,
para cualquier B. Memoria extra: 2P elementos, 2w bits y ⌈w/64⌉ palabras de prefijos.

**Verificación.** Exhaustiva para todas las entradas 0/1 con m ≤ 16 y P = 1..4 (524 280
casos, contra `std::stable_partition`); aleatoria para m hasta 4.2·10^6, incluido m no
múltiplo de P. Movimientos por elemento, pivote = mediana, B = 2P:

| m | B | m/B | actual | paginada |
|---|---|---|---|---|
| 2^16 | 2048 | 32 | 4.69 | 3.59 |
| 2^20 | 2048 | 512 | 6.71 | 3.50 |
| 2^22 | 2048 | 2048 | 7.70 | 3.51 |
| 3000017 | 2048 | 1465 | 8.03 | 3.50 |
| 2^20 | 65536 | 16 | 4.19 | 3.62 |
| 2^16 | 8192 | 8 | 3.74 | 3.62 |

La actual crece con lg(m/B); la paginada queda en ≈ 3.5 con mitades iguales.

**Limitaciones honestas.**
- La memoria de los bits es O(m/P): con P = 1024 y m = 10^9 son ~125 KB, más que el buffer
  fijo de 16 KB. El modo de memoria constante necesitaría P creciente o guardar los bits en
  el propio buffer (16 KB alcanzan para ~1.3·10^8 elementos con P = 1024).
- Con m/B ≤ 8 la actual ya es igual o más barata: hay que elegir entre las dos según m/B.
- 3.5 no es óptimo: la fase 2 mueve casi todas las páginas. Leer desde los dos extremos
  podría evitar parte de eso; no está analizado.
- Todavía no está integrada al quicksort ni medida en el harness de VM-Powersort.

## 4. Próximos pasos

1. ~~Demostración del invariante~~ y ~~prototipo~~ (sección 2b).
2. Derivar la constante c de la partición actual.
3. Elegir entre las dos particiones según m/B; resolver dónde viven los bits en modo O(1).
4. Integrar al quicksort y medir conteos (no tiempos) en el harness de VM-Powersort.
5. Seguir buscando antecedentes (memoria externa, distribution sort estable) antes de
   presentar la idea como propia.
