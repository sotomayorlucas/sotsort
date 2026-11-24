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

## 2c. Integración (opción `SOT_PAGED_PART`, apagada por defecto)

`stable_partition` usa la versión paginada cuando m > 8B y P = B/2 ≥ 64. Pasa la suite
completa (differential, extra, OOO, ASan+UBSan). Error encontrado al integrar: sin parcial
izquierda la fase 3 se autoasignaba cada elemento; con `std::string` eso vacía el contenido
(con enteros no se nota). Corregido en el prototipo y en la integración.

Conteos en el harness de VM-Powersort (`int`, misma memoria, n ≈ 10^6; resultados en
`bench/vmpowersort/results-pp/`). Solo cambia S = 2:

| S | comparaciones VM-PS / SotSort | movimientos VM-PS / base / paginada |
|---|---|---|
| 2 | 27.76 / 18.92 | 31.65 / 37.74 / 35.37 |
| ≥ 100 | sin cambios | sin cambios (no pasa por el quicksort) |

Desglose con un tipo contador y el mismo presupuesto (`bench/cost/breakdown.cpp`,
movimientos por elemento, entrada aleatoria):

| n | total | hojas | particiones | total con paginada | particiones con paginada |
|---|---|---|---|---|---|
| 10^6 | 40.04 | 6.73 | 33.24 | 37.35 | 30.55 |
| 10^7 | 51.78 | 6.73 | 44.98 | 44.93 | 38.13 |

Lectura: la mejora es real pero chica, porque con este presupuesto B ≈ 7·10^4 elementos
(n = 10^6) y solo los niveles altos tienen m/B > 8. El costo dominante es otro: **el
quicksort binario hace ~13 niveles de partición y cada uno cuesta ≥ 1.5 movimientos por
elemento** (los que van a la derecha pasan por el buffer y vuelven). VM-Powersort paga ~1.6
por nivel en ~20 niveles; SotSort paga ~2.5 por nivel en ~13, más las hojas. A cambio hace
32% menos comparaciones.

Consecuencia: para bajar movimientos hay que **bajar la cantidad de niveles**, no solo
abaratar cada partición. La partición paginada se generaliza a k vías sin cambiar su costo
por pasada (~3.5 movimientos por elemento; log2 k bits por página): 16 vías harían el
trabajo de 4 niveles binarios por ~3.5 movimientos, ≈ 0.9 por nivel binario, con ~log2 k
comparaciones por elemento por pasada. Es un samplesort estable al estilo IPS4o (que no es
estable). Hipótesis, no medida.

## 2d. Samplesort estable por páginas (prototipo, `bench/cost/paged_samplesort.cpp`)

Una pasada reparte en k grupos con k páginas de salida en el buffer (P = B/k); una página
llena se vuelca en la primera ranura libre y se anota su grupo. Los lemas 1–3 valen igual
(al volcar se leyeron ≥ (w+1)P elementos; el índice de ranura es el orden de volcado; al
final el espacio libre mide la suma de las parciales). Fase 2: permutación estable de
ranuras por grupo, por ciclos. Fase 3: copiar las parciales al buffer y correr cada grupo
hacia la derecha lo que suman las parciales de los grupos anteriores. Recursión por grupo;
los rangos que entran en el buffer van al quicksort de SotSort. Separadores: muestra de
8k elementos ordenada; grupo = upper_bound (≈ log2 k comparaciones).

Costo por pasada ≈ 4 movimientos por elemento (clasificar, volcar, permutar, correr) y
log2 k comparaciones, contra ≈ 2.5 movimientos y 1 comparación por nivel binario del
quicksort actual: con k = 64, ~0.7 movimientos por nivel binario.

Resultados (tipo de 8 bytes con contador, **presupuesto = el que reserva VM-Powersort para
este tipo**, estable verificado contra `std::stable_sort` en todos los casos):

| n | entrada | SotSort actual mov / cmp | paginado k=64 mov / cmp |
|---|---|---|---|
| 10^6 | aleatoria | 38.46 / 19.61 | 22.95 / 19.64 |
| 10^6 | 1000 claves | 26.54 / 11.31 | 11.33 / 11.42 |
| 10^7 | aleatoria | 48.32 / 22.94 | 26.30 / 23.05 |
| 10^7 | 1000 claves | 31.09 / 11.15 | 10.60 / 11.87 |

Referencia en el harness (con `int`, no con este tipo): VM-Powersort, S = 2, hace 31.65
movimientos / 27.76 comparaciones por elemento en n = 10^6 y 30.71 / 26.80 en n = 10^7.
Si se confirma en el harness, el punto débil (entrada casi aleatoria) pasaría a ser
favorable en las dos medidas.

Cuidado:
- Las tablas (grupo por ranura, origen, marcas) son O(n/P) palabras y todavía **no** se
  cuentan en el presupuesto; con k = 64 y n = 10^6 son ~1.4·10^3 ranuras (≈ 15 KB, ~5% del
  presupuesto). Se pueden comprimir (log2 k bits por ranura + select), como en §2b.
- Falta: comparar en el harness con los mismos tipos, entradas con estructura (S ≥ 100 no
  pasa por acá), adversarios para la muestra, grupos de iguales (como IPS4o), y medir que el
  número de pasadas sea el esperado.
- La idea de distribuir en páginas y permutar bloques es de IPS4o; lo que agrega esta
  versión es hacerla estable con el orden de volcado. No sé si eso ya existe.

## 2e. En el harness de VM-Powersort (`sotps`)

Implementación genérica en `bench/vmpowersort/paged_samplesort.h`. Ruteo igual que SotSort;
con entrada casi aleatoria, samplesort por páginas con k = 64. **Todo sale del presupuesto
de VM-Powersort**: buffer de páginas, tablas (2 + 4 bytes por ranura), muestra, separadores
y el `Sorter` de los casos base (con su buffer propio).

| tipo | S | comparaciones VM-PS / SotSort / paginado | movimientos VM-PS / SotSort / paginado |
|---|---|---|---|
| int | 2 | 27.76 / 18.92 / 18.98 | 31.65 / 37.74 / 22.14 |
| fastarr | 2 | 28.09 / 21.92 / 19.08 | 32.01 / 32.13 / 17.88 |
| int, n ≈ 10^7 | 2 | 26.07 / 21.86 / 21.92 | 29.90 / 48.18 / 26.49 |

Con S = 2 el paginado hace menos comparaciones **y** menos movimientos que VM-Powersort,
con la misma memoria. Con S ≥ 100 toma el camino de SotSort (mismos conteos ± 0.02
comparaciones del sondeo). El harness verifica el resultado en cada corrida.
Tiempo (indicativo, una máquina): int 1.05, ptr 0.84, fastarr 0.96, arr 0.61 respecto de
VM-Powersort.

## 2f. Demostración para k grupos

Notación: rango a[0, m), k grupos, páginas de P, k páginas de salida en el buffer, w =
páginas volcadas, f_g = ocupación de la página de salida del grupo g, grupo(x) = cantidad
de separadores ≤ x (los iguales caen en el mismo grupo).

**Lema 1' (siempre hay ranura).** Al volcar con w = f se leyeron q = fP + Σ_g f_g elementos
y algún f_g vale P, así que q ≥ (f+1)P. La ranura f está leída por completo y no contiene
páginas volcadas: escribir ahí no destruye nada.

**Lema 2' (orden).** Se vuelca siempre en la ranura w y w crece de a uno: el índice de
ranura es el orden de volcado. Cada página de salida recibe sus elementos en orden de
lectura. Entonces la secuencia estable del grupo g es: sus páginas por índice de ranura
creciente y luego su parcial. Basta anotar el grupo de cada ranura (⌈log2 k⌉ bits).

**Lema 3' (espacio final).** m = wP + Σ_g f_g, así que [wP, m) mide la suma de las
parciales, todo leído y sin volcar.

**Fases.** (1) Parciales a [wP, m) en orden de grupo. (2) Permutación de ranuras: destino
de una ranura del grupo g = F_g + (ranuras del grupo g con índice menor), con F_g = páginas
llenas de los grupos < g; es una biyección y conserva el orden dentro de cada grupo; por
ciclos con una página de buffer. (3) Parciales al buffer; de g = k−1 a 0, la parcial del
grupo g va a F_{g+1}P + D_g y sus páginas se corren D_g lugares, con D_g = Σ_{h<g} f_h.
Como D_g ≤ D_{g+1}, mover de derecha a izquierda no pisa nada sin mover.
Resultado: grupo g en [F_gP + D_g, F_{g+1}P + D_{g+1}), estable. ∎

**Costo exacto por pasada.** Comparaciones: Σ_x (pasos de la búsqueda binaria) ≤ m·⌈log2 k⌉.
Movimientos: m (clasificar) + wP (volcar) + Σf (fase 1) + (M + C)P (fase 2) + Σf (copiar
parciales) + Σ_{g≥1} (F_{g+1}−F_g)P (corrimiento, solo grupos con D_g > 0) + Σf
≤ 4m + 3kP, con M ≤ w páginas fuera de lugar y C ≤ M/2 ciclos (≤ 4.5m en el peor caso de
la fase 2). Memoria: kP elementos + w·(2 + 4) bytes de tablas en la implementación actual.

**Verificación.** Exhaustiva de la distribución: todas las entradas con claves en {0..k−1},
k = 2..4, P = 1..3, m ≤ 9 (1 140 207 casos), comparando posiciones y comienzos de grupo con
`std::stable_sort`. Sort completo: 81 casos (aleatoria, todos iguales, 2 y 64 claves,
sierra, casi todos iguales, permutación, descendente con iguales, un valor muy repetido;
n hasta 3·10^5; presupuestos de 20 KB a 1 MB), con ASan y UBSan
(`bench/cost/paged_samplesort_test.cpp`). Tope de pasadas: 2·log_k n + 2; después cae al
quicksort de SotSort, que tiene su propio respaldo. No probé adversarios contra la muestra.

## 2g. Adversario contra la muestra

`bench/cost/paged_samplesort_adversary.cpp`: adversario perezoso de McIlroy (responde
comparaciones sobre la marcha congelando valores) contra `pss::sort` con la semilla fija
del prototipo y el presupuesto de VM-Powersort; la entrada que construye se re-ejecuta con
el tipo contador. También se construye el adversario contra SotSort sin páginas.

Primera versión (solo tope de pasadas 2·log_k n + 2): n = 10^6, 95.4 comparaciones y 123.3
movimientos por elemento, contra 19.6 y 22.9 con entrada aleatoria.

Corrección: una pasada es mala si el grupo más grande supera m/2; dos malas seguidas en un
camino de la recursión mandan el rango al caso base (quicksort de SotSort, con su respaldo
por mezclas). Con eso (movimientos y comparaciones por elemento):

| n | entrada | paginado cmp / mov | SotSort cmp / mov |
|---|---|---|---|
| 10^5 | aleatoria | 16.30 / 17.68 | 16.45 / 30.35 |
| 10^5 | adversario del paginado | 35.74 / 68.30 | 14.08 / 21.14 |
| 10^5 | adversario de SotSort | 14.15 / 14.40 | 25.45 / 64.91 |
| 10^6 | aleatoria | 19.64 / 22.94 | 19.62 / 44.88 |
| 10^6 | adversario del paginado | 42.00 / 96.70 | 8.58 / 22.80 |
| 10^6 | adversario de SotSort | 30.08 / 95.59 | 30.06 / 95.59 |

Lectura: el peor caso construido queda en el orden del peor caso del propio SotSort
(≈ 96 movimientos por elemento en 10^6), porque el adversario empuja al caso base y ataca
el quicksort de abajo. No es un peor caso garantizado bueno: VM-Powersort, como mergesort,
no tiene esta debilidad. La entrada construida depende de la semilla: con otra semilla la
misma entrada cuesta 17.37 / 17.74 en 10^6. Con semilla secreta por llamada (SotSort ya la
admite en `Options::seed`) el ataque necesita conocerla.
Los conteos con entrada aleatoria en el harness no cambian (int 18.98 / 22.14).

## 2h. Presupuesto de particiones malas del quicksort de SotSort

El peor caso del 2g venía del quicksort de abajo: toleraba lg n particiones malas (lado
menor < len/16) antes de pasar a la mezcla de respaldo, y cada una cuesta ~2.5–4
movimientos por elemento sin achicar el rango. Cambio en `include/sotsort.hpp`: el
presupuesto pasa a min(lg n, 4) (`SOT_BAD_BUDGET`, por defecto 4).

Movimientos por elemento, n = 10^6, presupuesto de VM-Powersort (`bench/cost/`):

| entrada | presupuesto lg n | 4 | 2 |
|---|---|---|---|
| adversario contra SotSort / SotSort | 95.59 | 29.28 | 21.00 |
| adversario contra el paginado / paginado | 96.70 | 34.06 | 26.23 |
| aleatoria, dup16, sqrtn, zipf, organ, runs63, tail10, asc1pct, intercalada | sin cambios | sin cambios | sin cambios |

(Con comparaciones: 30.06 → 14.18 → 12.19 y 42.00 → 26.13 → 24.14.) En las nueve
distribuciones los conteos son idénticos con los tres presupuestos, en modo O(1) y con
buffer: el presupuesto nunca se agota fuera del adversario. Elegí 4 y no 2 por margen
para entradas legítimas con muchas particiones desparejas, que no medí. La suite completa
pasa (differential, extra, OOO, memoria, ASan+UBSan) y los conteos del harness no cambian.
La garantía O(n log n) no cambia: el respaldo es el mismo, solo llega antes.

## 2i. Integración en SotSort (v8-r6)

`Sorter::pss_sort` en `include/sotsort.hpp`: camino de entrada casi aleatoria en modo
bloques (buffer B con 2B < n). k ∈ {64, 32, 16, 8}, el mayor con páginas P = ⌊(B−k+1)/k⌋ ≥ 64
y tablas (6 bytes por ranura) dentro de la reserva de tablas del plan (B·s/8 + 4 KB), que ya
se descontaba del presupuesto. Separadores en las últimas k−1 posiciones del buffer; la muestra
se ordena en el área de páginas. Caso base: el quicksort con tope de 4 particiones malas.
La regla de elementos grandes (≥ 64 bytes → mezcla) solo se aplica si este camino no está
disponible. La partición estable de 2 vías por páginas (`SOT_PAGED_PART`) se retiró del
encabezado: el samplesort la reemplaza; el prototipo queda en `bench/cost/`.
Tests: suite completa + `tests/test_pss.cpp` (tipos de 112 bytes y `std::string`, presupuestos
de 200 KB a 4 MB, claves aleatorias, 3 y 1000 valores) + ASan/UBSan/TSan.

**Cota de peor caso (movimientos).** Cada pasada cuesta ≤ 4.5m + O(kP) movimientos y
m⌈log2 k⌉ comparaciones; un camino de la recursión hace a lo sumo 2·log_k n + 2 pasadas (o
termina antes por dos pasadas malas seguidas). El caso base es el quicksort con a lo sumo
4 particiones malas por camino antes del respaldo por mezclas (O(n log n) movimientos con
buffer). Total O(n log n) comparaciones y movimientos en el peor caso; las constantes del
peor caso medido con el adversario de McIlroy están en §2g–2h. No es una cota ajustada.

## 2j. Clasificación sin saltos

La búsqueda binaria entre separadores tenía un salto impredecible por nivel. Ahora los
separadores forman un árbol implícito (nodo i, hijos 2i y 2i+1, recorrido en orden = orden)
y el grupo se obtiene con b = 2b + (separador ≤ x) durante log2 k pasos, como el árbol de
decisión de IPS4o. Mismos conteos (k es potencia de 2). Tiempo respecto de VM-Powersort con
S = 2, A/B en el mismo binario, nube / PC: int 1.01 → 0.72 / 1.11 → 0.78; ptr 0.88 → 0.82 /
0.90 → 0.86; fastarr 0.89 → 0.73 / 0.95 → 0.73; arr 0.64 → 0.55 / 0.52 → 0.57.

## 3. Estado de la literatura (búsqueda del 19 nov 2026, no exhaustiva)

**Antecedente directo: Radsort** (Clausecker y Schintke, "Parallel O(√n) Overhead LSD Radix
Sort", arXiv:2607.05302, 2026). Ordena estable por radix LSD con bloques: cada elemento va
al bloque de salida de su bucket; un bloque lleno ocupa el siguiente bloque lógico ya
consumido de la entrada (su Lema 1 es nuestro Lema 1'); un arreglo U anota el bucket de
cada bloque de salida y una permutación π dice dónde vive cada bloque lógico; entre rondas
no se mueven datos, solo índices, y al final se ubican los bloques. Memoria O(b + n/b).
**El mecanismo central de nuestra pasada (reusar bloques consumidos y anotar el grupo de
cada bloque para conservar la estabilidad) ya está ahí.** Sus autores dejan el MSD como
trabajo futuro porque los bloques parciales recursivos les costaban demasiada memoria.

Lo que nuestra versión hace distinto (contribución posible, a validar):
- comparaciones con separadores muestreados (samplesort), no radix: sirve para cualquier
  tipo con un orden;
- MSD recursivo: cada pasada deja los grupos físicamente contiguos (permutación por ciclos
  + corrimiento de parciales, ~1.5 movimientos extra por elemento) y la recursión trabaja
  sobre rangos reales; eso resuelve el problema de las parciales que Radsort menciona;
- integrado como camino de entrada casi aleatoria de un sort adaptativo con presupuesto fijo
  de memoria, con conteos contra VM-Powersort.

**Antecedente directo de la versión de 2 vías: Logsort** (usuario aphitorite, GitHub,
licencia MIT; fecha no indicada). Quicksort estable in-place en O(n log n): con un buffer de
O(log n) agrupa los 0 y 1 de la partición en bloques de un solo tipo, codifica el orden de
los bloques intercambiando elementos (O(log n) bits por par de bloques, sin tablas), lleva los
bloques de la partición mayor a su lugar y reordena los otros por ciclos, como cycle sort.
Es la misma familia que nuestra partición de 2 vías (§2b), con codificación implícita en vez
de bits en una tabla; no tiene respaldo ante pivotes malos y su autor no lo describe como
adaptativo. Debería figurar en las comparaciones de SotSort (no estaba).

Polychroniou y Ross (SIGMOD 2014, "A comprehensive study of main-memory partitioning..."):
particionado fuera de caché in-place "en listas de bloques" y "en segmentos"; con eso arman un
LSB radix estable (no in-place), un MSB radix in-place y un sort por comparaciones con
particionado de rango de alto fan-out. Solo leí el resumen y las diapositivas; no sé si el
particionado in-place por listas de bloques conserva el orden. Pendiente leer el paper.

Otros:
- IPS4o / IPS2Ra (Axtmann, Witt, Ferizovic, Sanders): distribución por bloques con buffers y
  permutación de bloques; no preserva el orden de los bloques dentro de un bucket, no es
  estable.
- Polychroniou y Ross: particionado en memoria con listas de bloques; Radsort lo cita como lo
  más parecido. No lo leí.
- Katajainen y Pasanen (1992): partición estable en O(n) y O(1) espacio; teórico.
- Xu y Chick, "Near-Optimal In-Place Sorting" (arXiv:2607.27040): buffer, segmentos y vector
  de bits; inestable.
- VM-Powersort: páginas para mezclas.

**Búsqueda a fondo (20 nov 2026).** Wild (respuesta del 20 nov): no conoce de memoria una
versión MSD por comparaciones. Lo encontrado:
- **Polychroniou y Ross (SIGMOD 2014), §3.2.3, "out-of-cache in-place, list of blocks":** cada
  partición es una lista enlazada de bloques de un solo destino; los bloques llenos ocupan espacio
  de la entrada ya leído (antes se apartan P·B tuplas del comienzo). El orden de cada partición
  queda en la lista. El paper no discute la estabilidad de esta variante; afirma en general que
  el particionado in-place no es estable, y ninguno de sus sorts combina in-place, estable y por
  bloques. Radsort dice que su diseño es parecido a este, con arreglos en vez de listas.
  **Es el antecedente más viejo del mecanismo** (reusar espacio leído + registrar el orden).
- **Franceschini, Muthukrishnan y Pătraşcu (ESA 2007), "Radix sorting with no extra space":** su
  mezcla de k listas con αn palabras libres escribe la salida en bloques libres y toma bloques que
  se liberan en las listas de entrada. Mismo truco de reuso, para mezclar.
- **IPS4o:** vuelca los bloques en la entrada ya distribuida y explícitamente *no* registra a qué
  bucket pertenece cada bloque, para ser in-place; luego permuta. Por eso no es estable. Registrar
  el bucket (O(n/b) palabras) es justamente lo que da la estabilidad.
- **Katajainen y Pasanen (1992, 1994):** partición estable y sort de multiconjuntos estable en
  espacio mínimo; teóricos, quicksort con mediana.
- **Memoria externa:** distribution sort escribe cada bucket en bloques (clásico; Vitter). Las
  implementaciones con listas de bloques por bucket son estables por construcción, pero no
  trabajan sobre el mismo arreglo.
- No encontré: un samplesort por comparaciones, MSD, estable, que reuse el espacio leído y deje los
  grupos contiguos en cada pasada. Búsqueda en buscadores generales; no revisé bases de patentes
  ni todas las citas de IPS4o.

Conclusión honesta: la idea no es nueva en su núcleo. Reusar espacio ya leído y registrar el orden
de los bloques está en Polychroniou y Ross (listas de bloques, 2014) y en Radsort (k vías, radix
LSD, con arreglos); agrupar en bloques de un tipo y
reordenarlos por ciclos con el orden registrado está en Logsort (2 vías, comparaciones).
Lo presentable es la combinación (samplesort de k vías por comparaciones, MSD con grupos
contiguos en cada pasada, dentro de un sort adaptativo con memoria acotada) y las mediciones,
citando a los dos.

## 4. Próximos pasos

1. ~~Demostración del invariante~~ y ~~prototipo~~ (sección 2b).
2. Derivar la constante c de la partición actual.
3. Elegir entre las dos particiones según m/B; resolver dónde viven los bits en modo O(1).
4. Integrar al quicksort y medir conteos (no tiempos) en el harness de VM-Powersort.
5. Seguir buscando antecedentes (memoria externa, distribution sort estable) antes de
   presentar la idea como propia.

## 2k. Conteos contra Logsort (20 nov 2026)

`bench/cost/logsort_counts.cpp` (Logsort de aphitorite, commit 76a5e32, en `bench/cost/logsort/`
con los `memcpy` reemplazados por asignaciones para contar). Tipo de 16 bytes; Logsort con bLen =
buffer propio de SotSort en modo O(1) (1024 elementos, 16 KB) y con el presupuesto de VM-Powersort.
Resultados completos en `bench/cost/logsort_counts_1e6.txt` y `_1e7.txt`.

- Con presupuesto VM, aleatoria, 10^7: Logsort 23.42 / 73.22, SotSort 22.94 / 22.94 (cmp / mov).
- Con 16 KB, aleatoria, 10^7: Logsort 23.52 / 99.24, SotSort 22.95 / 111.65. En 10^6 gana
  SotSort (78.49 contra 73.18 movimientos). Con muchas claves iguales Logsort mueve menos.
- **Costo del paginado con iguales:** dup16 en 10^7 pasa de 5.19 / 25.72 (sin paginado) a
  14.01 / 7.95. Falta darle grupo propio a los iguales (como IPS4o).
- No es del paginado: con presupuesto, runs63 y asc1pct hacen 17.7 y 16.8 comparaciones en 10^7
  contra 2.2 y 1.8 en modo O(1); pasa igual con `-DSOT_NO_PAGED_SAMPLE`. Es el ruteo con buffer.
- interl (mitades intercaladas): SotSort hace ~20.7 comparaciones contra ~16 de Logsort.

## 2l. Grupos de iguales (v8-r7)

Si dos de los k−1 separadores elegidos de la muestra son iguales, la pasada usa un árbol de k/2
hojas y 2(k/2)−1 = k−1 grupos: el rango g−1, el grupo de los iguales a s_{g−1} y el rango g,
como los buckets de igualdad de IPS4o. Grupo = 2g − [g > 0 y x == s_{g−1}], con x == s_{g−1}
probado como !(s_{g−1} < x) (una comparación más; el árbol tiene un nivel menos: siguen siendo
log2 k por elemento). Los separadores en orden van detrás del árbol (k−2 ≤ k−1 lugares). Los
grupos de iguales quedan terminados y no se recursan; la regla de pasadas malas mira solo los
rangos. Los lemas 1'–3' no cambian: solo cambia la función de grupo.

Conteos (n = 10^7, tipo de 16 bytes, presupuesto VM, cmp / mov): dup16 14.01 / 7.95 → 6.00 / 3.94;
zipf 14.46 / 8.12 → 7.09 / 4.36; sqrtn 14.18 / 10.31 → 12.91 / 9.02; aleatoria sin cambios
(22.94 / 22.94). Zipf sigue por encima del quicksort sin paginado en comparaciones (4.94): solo
reciben grupo los valores que aparecen repetidos entre los separadores elegidos (IPS4o además
elimina duplicados de la muestra; no lo hice).
Tests: suite completa; `tests/test_pss.cpp` ahora con 1, 2, 3, 16, 64 y 1000 valores, claves
geométricas y mitad un valor / mitad distintos (108 casos), también con ASan y UBSan. El adversario
de McIlroy da lo mismo que antes: congela valores distintos, así que no activa este modo.

## 2m. Galope en la mezcla paginada de runs (v8-r7)

Causa del hueco de 2k: con buffer acotado, runs63 y asc1pct van a `paged_natural_merge_sort`
(esquema de VM-Powersort), cuya mezcla página a página no galopaba; en modo O(1) la mezcla común
sí galopa. Arreglo: `merge_page_gallop`, galope estilo Timsort (umbral MG = 7) dentro de cada
página de X, de Y y de salida; el estado sigue de una página a la siguiente y un galope cortado
por el borde no cuenta como corto. Se decide por mezcla con la sonda de la mezcla común (8
muestras de X ubicadas en Y), para todos los tipos: sin sonda, en el harness de VM-Powersort
(tipo envoltorio no trivial, así que la sonda de la mezcla común no corre) S = 1000 pasaba de
9.71 a 9.86 comparaciones y quedaba arriba de VM-Powersort (9.72).

Conteos (tipo de 16 bytes, presupuesto VM, n = 10^7, cmp / mov): runs63 17.70 / 20.21 → 7.13 / 20.21;
asc1pct 16.76 / 19.21 → 4.74 / 19.21; tail10 3.99 → 4.12; el resto igual. Sin sonda daban 5.40 y
4.34: la sonda cuesta algo en estas entradas. Harness de VM-Powersort, int, n ≤ 10^6, totales/10^6
(`bench/vmpowersort/results-r7`): comparaciones r6 → r7 en S = 100: 13.09 → 13.18; 1000: 9.71 →
9.74 (VM-PS 9.72); 10000: 6.78 → 6.74; las demás iguales; asignaciones iguales en todas.
Tests: suite completa, `make sanitize`, y `logsort_counts.cpp` con ASan/UBSan en n = 2·10^5 y 10^6
(9 distribuciones, salida ordenada y estable).

## 2n. v8-r8: vías según la muestra, subrangos ya ordenados, galope adaptativo (22 nov 2026)

Tres cambios, para las debilidades que quedaban en 2k–2m.

1. **Separadores y vías según la muestra** (`pss_choose`). La muestra ordenada se comprime en
   valores distintos con su cantidad. Para cada kt en {k/2, ..., 2}: separadores = valores de las
   posiciones equidistantes, sin repetir; los lugares que sobran, a los valores más frecuentes
   que falten (si no alcanzan los valores, se repite el último: quedan rangos vacíos). Costo
   estimado por elemento: log2 kt + 1 + u·resto, con u = fracción de la muestra que no es
   separador y resto = (log2 max(1, (D − separadores)/kt) + 2)·(1 + 1/log2 k); D = valores
   distintos estimados con Chao1 (vistos + f1²/(2·f2)). Sin iguales: log2 k + resto con cero
   separadores. Gana el menor. Es una heurística: todas las opciones ordenan bien. Dos versiones
   previas del modelo (R = log2 de los valores vistos; R = log2 m si la muestra no satura)
   empeoraban sqrtn (13.5–13.8 comparaciones contra 12.9 de r7) y se descartaron.
2. **Subrangos ya ordenados** (`ascending`): tras cada pasada paginada, un rango en orden no
   decreciente no se recursa, y uno estrictamente descendente se invierte. En el quicksort, lo
   mismo para subrangos de SOT_SORTED_MIN = 1024 o más que salen de una partición; las
   comparaciones de chequeos fallidos se cortan en un cuarto del rango inicial de la llamada.
3. **Galope adaptativo en la mezcla paginada**: si la sonda dice que no (o la mezcla tiene menos
   de 4096 elementos y la sonda no se paga), la mezcla empieza de a uno y galopa tras una racha
   de mg (umbral estilo Timsort: +2 al salir del galope, −1 por ronda larga). La cota de la sonda
   es floja con muchas claves iguales (dientes de sierra: 18.27 → 6.73 comparaciones).

Conteos (16 bytes, n = 10^7, presupuesto VM, cmp / mov; r7 → r8): zipf 7.09 / 4.36 → 5.28 / 4.70;
dup16 6.00 / 3.94 → 5.00 / 3.94; sqrtn 12.91 / 9.02 → 12.56 / 8.37; interl 20.72 / 18.27 →
7.14 / 4.06; runs63 7.13 → 5.45; asc1pct 4.74 → 4.38; aleatoria igual (22.94 / 22.94). Modo O(1):
interl 20.68 / 35.45 → 12.03 / 24.62; zipf 4.94 → 4.35; dup16 5.19 → 5.00 (rangos de iguales
cuentan como ordenados). Más entradas en `bench/cost/extra_dists_1e7.txt` (r7 y r8): tres
secuencias intercaladas 20.74 / 18.31 → 7.29 / 4.15; descendentes intercaladas 20.24 / 23.10 →
7.12 / 5.56; 90% un valor 7.80 → 3.96; dos valores 6.00 → 3.00. Empeora en movimientos:
40% un valor + distintos 16.80 / 15.90 → 15.48 / 18.20; mitad un valor 14.99 / 13.73 → 12.97 / 14.64.
Harness de VM-Powersort (int, n ≤ 10^6, `bench/vmpowersort/results-r8`), comparaciones/10^6:
S = 2: 18.323 (r6 18.319); 100: 13.120 (r6 13.087, r7 13.176); 1000: 9.707 (VM-PS 9.718);
10000: 6.746; 10^5: 3.794; 10^6: 1.613 (VM-PS 1.608). Asignaciones idénticas a r6.
Adversario de McIlroy contra SotSort (n = 10^6): 26.05 / 35.22 (antes 26.13 / 35.46).
Tests: suite, `make sanitize`, `test_pss` con ASan/UBSan, y `logsort_counts` y `extra_dists`
con ASan/UBSan en n = 10^6 (19 distribuciones, salida ordenada y estable).
No hecho: un adversario contra el modelo de costo de `pss_choose`.

## 2o. v8-r9: tiempos de r8 y regresión corregida (24 nov 2026)

Tiempos A/B en el mismo binario, ambos órdenes (`bench/vmpowersort/ab_r8.sh`, resumen con
`ab_summary.py`; nube, 2 núcleos, n = 10^6, 10 iteraciones; sot6vm = r6 con búsqueda binaria).
**r8 tenía una regresión de tiempo con runs aleatorios:** S = 100, r8/r6 = 1.23 (int), 1.53 (ptr),
1.13 (fastarr), 1.40 (arr), y quedaba arriba de VM-Powersort en int (1.05). Causa: el lazo de a uno
con contadores de racha (galope adaptativo de 2n) es más lento que `merge_run_k`. Los conteos no
lo mostraban (13.12 contra 13.09 comparaciones).
Arreglo: sin sonda a favor, la mezcla usa `merge_run_k`; alterna un tramo de detección de 16
salidas con tramos de hasta 240 sin mirar, y pasa a galopar si en el tramo de 16 salen 14 o más de
un mismo lado; vuelve cuando los galopes salen cortos. (Tramos de 16 siempre: todavía +13% en int.)
Tiempos r9, razón contra VM-Powersort (r6 al lado), media de los dos órdenes:
S = 2: int 0.75 (1.03), ptr 0.73 (0.80), fastarr 0.74 (0.91), arr 0.53 (0.61);
S = 100: int 0.93 (0.87), ptr 0.74 (0.69), fastarr 0.84 (0.85), arr 0.84 (0.84);
S = 10^6: int 0.71 (0.70), ptr 1.00 (1.02), fastarr 0.57 (0.60), arr 0.78 (0.80).
En S = 100 quedan +6–7% en int y ptr respecto de r6, dentro de la variación entre órdenes (±5–8%).
Conteos r9 (`results-r9`): comparaciones S = 100: 13.112; 1000: 9.713 (VM-PS 9.718); 10000: 6.747;
asignaciones idénticas a r6. runs63 5.62, asc1pct 4.54, dientes de sierra 6.74 (r8: 5.45, 4.38, 6.73).
La PC no estaba conectada: los tiempos de la segunda máquina siguen siendo los de r6.
