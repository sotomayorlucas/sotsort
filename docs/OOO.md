# SotSort-OOO: auditoría, diseño, análisis y resultado experimental

Base: `sotsort.hpp` v8-r (reconstrucción). Prototipo: `sotsort_ooo.hpp`, aislado; la API de v8-r
no cambia. Máquina: VM de 2 vCPU. **Todo lo medido vale para P = 2; no hay datos de escalabilidad.**

## 1. Auditoría: dependencias reales

| Operación | Lee / escribe | Depende de | Independiente de |
|---|---|---|---|
| QuickEstable sobre [l,r) | A[l..r), buffer | la partición que creó [l,r) | todo rango disjunto |
| ParticiónBloque(bloque i) | bloque i, buffer (<= B) | nada | los otros bloques |
| Fusionar(L,R) = Rotar | A[corte(L)..corte(R)), buffer | bloques y fusiones de L y de R | subárboles disjuntos |
| SymMerge / mezcla de runs | los dos runs, buffer | que los dos runs estén ordenados | pares disjuntos |
| Merge path (ronda) | S (lectura), D por tramos | la ronda anterior; cortes antes de mover | tareas de la misma ronda |
| Runs naturales | detección secuencial | orden de descubrimiento (pila de potencias) | — |

- El **buffer compartido** es una dependencia falsa (escritura/escritura), como un registro
  reutilizado. Se elimina con "renombrado": un buffer por tarea o por trabajador.
- **Rotar** escribe el intervalo completo [corte(L), corte(R)): nada dentro de L ∪ R puede correr
  a la vez. Las fusiones forman un árbol; solo los subárboles disjuntos son paralelos.
- **Movimientos**: con tipos no triviales, mover vacía el origen. Por eso merge path calcula todos
  los cortes antes de mover (barrera necesaria dentro de un par, no entre pares).
- La barrera **entre pares** de una ronda sí es innecesaria: un par puede mezclarse apenas sus dos
  runs están listos. Solo rinde si los trozos tardan distinto.
- El estado del generador de pivotes es otra dependencia falsa: se deriva de (semilla, lo, hi).

## 2. DAG y modelo

Tarea = rango pendiente de QuickEstable, con su presupuesto y su cota inferior. Arista = "la
partición de la tarea madre terminó". El DAG es un **árbol** (inclusión de intervalos). No hay
esperas entre tareas (ninguna tarea hace join), así que no hace falta guardar el grafo: alcanza
una cola de tareas listas, acotada en 64·P. Con la cola llena, la mitad grande queda en la pila
local, como en el algoritmo secuencial.

Memoria: modo Completo, la tarea [lo,hi) usa aux[lo..hi) (n elementos en total, igual que v8).
Modos Bloques y O(1): un buffer por trabajador. Variante de memoria constante: P·B0 elementos más
la cola (64·P tareas) y P pilas de O(log n): O(P log n) palabras, constante para P fijo salvo el
factor log n de las pilas, que ya existe en la versión secuencial.

## 3. Trabajo, camino crítico, paralelismo

- W(n): el del quicksort secuencial más O(1) por tarea. Comparaciones medidas por elemento
  (n = 10^6): aleatoria 20,52 secuencial y 20,52 OOO; 16 valores 5,33 y 5,45.
- S(n): la partición de un rango de largo λ es secuencial, Θ(λ). Siguiendo siempre la mitad
  grande, S(n) = Θ(n) esperado. Paralelismo W/S = Θ(log n): unas 10 veces para n = 10^6.
- Con P = 2 la raíz secuencial cuesta: fracción secuencial ≈ 1/lg(n/L) ≈ 1/13, tope ≈ 1,86×.
- ParticiónEstable por bloques (modos O(1) y Bloques): W = Θ(λ lg(λ/B)); con rotaciones
  secuenciales la fusión de la raíz mueve Θ(λ), así que S = Θ(λ) y el paralelismo interno es
  O(lg(λ/B)). No alcanza para justificar tareas de 16 KB.

## 4. Corrección

**Demostrado (reutiliza el libro):**
1. *Unicidad.* Para un orden débil estricto, la permutación estable ordenada Ω es única. Toda
   ejecución que termina con A ordenado y coherente da A = Ω: la salida no depende del orden de
   ejecución, y coincide con la de v8.
2. *Invariante.* El de QuickEstable (sección 1.8) habla de un **conjunto** de intervalos
   pendientes, no de una pila: "[0,n) se descompone en intervalos disjuntos, cada uno terminado o
   pendiente; si un pendiente tiene lb, ninguno de sus elementos es menor que lb". Vale igual si
   los pendientes están repartidos entre la cola, las pilas locales y los rangos en curso. Cada
   paso de una tarea es un paso del lazo secuencial sobre su propio intervalo.
3. *Conmutación.* Condiciones de Bernstein: si W(Ti) ∩ (R(Tj) ∪ W(Tj)) = ∅ y recíprocamente,
   Ti(Tj(A)) = Tj(Ti(A)). Dos tareas vivas tienen intervalos disjuntos (invariante) y buffers
   disjuntos (por construcción), así que conmutan.
4. *Aciclicidad.* Las dependencias siguen la inclusión estricta de intervalos.
5. *Progreso.* Ninguna tarea espera a otra; cada una termina por el lema 1.18; la cola es FIFO y
   el número de tareas es finito. No hay deadlock ni inanición.

**Argumento informal (no escrito con el detalle del libro):**
- Buffers disjuntos en modo Completo: ahora es el lema B de `LEMAS_v8r.md` (a mano, sin mecanizar).
- Ausencia de carreras en el planificador: todo su estado se toca con el mutex tomado.

**Verificado solo por pruebas:** 933 casos contra `std::stable_sort` con etiquetas, 2 a 5 hilos,
grano de 64 a 4096, los tres modos de memoria; ThreadSanitizer y ASan+UBSan sin avisos.
**Comparador que lanza dentro de una tarea:** 60 corridas con strings, 2 a 4 hilos, ASan+UBSan y TSan: sin cuelgues ni errores de memoria (se pueden perder elementos, como en el modo secuencial).
**Sin verificar:** más de 2 núcleos reales.

## 5. Resultados (P = 2, mediana de 5 a 7; segunda versión del prototipo)

Segunda versión: partición paralela de la raíz (lema R en `LEMAS_v8r.md`), delegación sin repetir
recorridos y conteo de giros en paralelo. Cociente OOO / paralelo v8-r (menos de 1 gana OOO):

| Entrada | u64, 10^6 | u64, 10^7 | 128 B, 250 mil | 128 B, 10^6 | strings, 250 mil | strings, 10^6 |
|---|---|---|---|---|---|---|
| aleatoria | 0,82 | 0,94 | 0,68 | 1,13 | 0,60 | 0,93 |
| 16 valores | 0,63 | 0,89 | 0,70 | 1,08 | 0,61 | 1,10 |
| √n valores | 0,80 | 1,03 | 0,67 | 1,06 | 0,57 | 1,04 |
| mitad un solo valor | 0,91 | 0,83 | 0,72 | 0,89 | 0,75 | 0,89 |
| mitades desbalanceadas | 0,68 | 0,77 | 0,77 | 1,04 | 0,65 | 0,65 |
| 1% de ruido (delegado) | 1,07 | 0,97 | 1,01 | 1,05 | 1,09 | 1,07 |
| organ pipe (delegado) | 1,25 | 1,18 | 1,00 | 1,10 | 1,03 | 1,11 |

- En las columnas de 250 mil el paralelo de v8 no se activa (exige n ≥ 2^18): ahí OOO se compara
  contra una ejecución de un hilo.
- Aceleración de OOO sobre un hilo en las entradas que van por tareas: 1,24× a 1,93×.
- Efecto de la raíz paralela (OOO con raíz / OOO sin raíz): entre 0,79 y 1,16; mediana cerca de
  0,96. Ayuda con registros de 128 B (0,87 a 0,95) y casi nada con u64 de 10^6.
- Planificador: 3 a 249 tareas por corrida. En la primera versión la cola máxima fue 81 y no hubo rechazos; en esta no se registró. Con la raíz paralela
  el ocio bajó de 20–60 ms a menos de 6 ms en u64 de 10^7.
- Comparaciones por elemento (n = 10^6, comparador contado): aleatoria 20,52 secuencial, 20,53
  paralelo, 21,32 OOO; 16 valores 5,33 / 6,36 / 6,26. El exceso de OOO es el conteo de giros, que
  en paralelo ya no corta temprano.
- Entradas delegadas: 0 a 25% más lentas que el paralelo, por ese mismo conteo.
- El ruido entre corridas idénticas en esta máquina llegó a ±15%: diferencias menores no dicen nada.
- No medido: movimientos, fallos de caché, ancho de banda, falsa compartición, P > 2.

## 6. Decisión

- **Planificación por tareas del quicksort + raíz paralela: implementada; queda experimental.**
  Gana con claridad donde v8 no paraleliza (n < 2^18) y con entradas desbalanceadas (0,65 a 0,77).
  En el resto queda dentro del ruido, entre 0,83 y 1,13. No reemplaza al modo paralelo: no hay
  una mejora reproducible en la región principal y solo se pudo medir con 2 núcleos.
- **Candidato concreto a incorporar:** usar tareas cuando n < 2^18 (hoy v8 corre con un hilo).
  Es la única región con ventaja grande y consistente (0,57 a 0,91).
- **Partición estable fuera de orden por bloques y rotaciones: descartada sin prototipo.** El
  paralelismo interno es O(lg(λ/B)) y el camino crítico lo fija la rotación de la raíz.
- **Pendiente para decidir de verdad:** medir en 8 núcleos o más; partición paralela también en
  los niveles 2 y 3; conteo de giros con corte temprano.

## 7. Antecedentes (de memoria; verificar las citas antes de publicar)
- Condiciones de independencia: Bernstein, 1966.
- Robo de trabajo y modelo trabajo/camino crítico: Blumofe y Leiserson (Cilk).
- Tareas con dependencias inspiradas en procesadores superescalares, con renombrado: StarSs /
  OmpSs (BSC); cláusula `depend` de OpenMP.
- Quicksort paralelo por tareas: TBB `parallel_sort`, modo paralelo de libstdc++.
- Mezclas que empiezan antes de que terminen las de abajo: mergesort en tubería de Cole (1988).
- Sort paralelo in-place: IPS4o (Axtmann, Witt, Ferizovic, Sanders).

Lo específico de esta adaptación sería poco: reutilizar el invariante de QuickEstable sin cambios
y el renombrado gratis del buffer en modo Completo. No es un aporte publicable por sí solo.
