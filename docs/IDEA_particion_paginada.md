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

Conclusión honesta: la idea no es nueva en su núcleo. Lo presentable es la combinación
(samplesort por comparaciones, MSD con grupos contiguos, dentro de un sort adaptativo con
memoria acotada) y las mediciones, citando Radsort como origen del mecanismo.

## 4. Próximos pasos

1. ~~Demostración del invariante~~ y ~~prototipo~~ (sección 2b).
2. Derivar la constante c de la partición actual.
3. Elegir entre las dos particiones según m/B; resolver dónde viven los bits en modo O(1).
4. Integrar al quicksort y medir conteos (no tiempos) en el harness de VM-Powersort.
5. Seguir buscando antecedentes (memoria externa, distribution sort estable) antes de
   presentar la idea como propia.
