# SotSort v8-r: reconstrucción (17-nov-2026)

El `stable_qsort.hpp` de la versión 8 se perdió. `sotsort.hpp` es una reescritura, no el
mismo código. **Ningún número medido con la v8 original vale para este archivo.**

## Fuentes usadas
- `sq5.hpp`: código real de una versión anterior (rotación, merges con buffer, SymMerge, hojas).
- `sotsort_demo.tex` (v6.1), libro (v7), `v8_algs.tex` / `demo_v8.tex` / `cap2_v8.tex` (v8).
- `engine.js` del visor (v6.1: runs naturales, cota inferior, hojas por potencias de 2).

## Qué sigue los documentos
Detección de orden, pocos giros (corte a n/8), runs naturales con criterio de potencias,
QuickEstable con cota inferior y presupuesto (línea de v6.1 incluida), partición por bloques con
fusión en orden de contador binario, partición vectorial (AVX-512 con compresión; AVX2 con tabla
de 16 filas), rotación (buffer, puente, triple reversión fusionada), parity merge de largos
distintos, plan de memoria (O1 / Bloques / Completo, `memory_known`, caída escalonada,
2 MB + MADV_HUGEPAGE), conteo antes de reservar el buffer, galope (M_G = 7), co-rango y modo
paralelo con cortes precalculados, orden por clave (indirecto empaquetado y radix LSD).

## Qué es una decisión mía (los documentos no lo fijan)
- Muestreo del pivote: estratificado con offset pseudoaleatorio por estrato (9 estratos para el
  ninther). El libro dice "muestreo aleatorio" sin más detalle.
- Constantes: B0 = max(128, 16 KB / sizeof(T)), LEAF = 128, RUN = 24, N9 = 8192.
- La pila de rangos de QuickEstable va al heap si sizeof(T) > 64.
- Nombres de la API (`sot::stable_sort`, `stable_sort_fast`, `stable_sort_by_key`, `Options`).

## Qué es NUEVO respecto de los documentos (no cubierto por las demostraciones tal cual)
- **Sonda de rachas** en SymMerge (camino de runs naturales, tipos trivialmente copiables, cuando
  los dos runs entran en el buffer): elige entre parity merge y mezcla con galope. Las dos mezclas
  ya están demostradas por separado; la sonda solo cambia cuál se usa. Medido (u64, n = 10^6,
  buffer completo, mínimo de 9): sierra de runs de 1000: 79 -> 31 ms; runs de 63: 112 -> 43 ms;
  1% de ruido: 10,6 -> 12,3 ms (empeora 16%).
- `Sorter::Spawner`, `quick_task`, `follow_buffer`: ganchos para el prototipo OOO.
- `SOT_V6_BUG`: quita la línea de v6.1 para reproducir el defecto.

## Segunda tanda de optimizaciones (v8-r, misma fecha)
Medidas contra la reconstrucción inicial con corridas intercaladas (`results/ab_v8r_vs_v8r2.txt`).
- **Sonda de rachas extendida:** ahora decide entre galope, parity merge y lazo clásico en toda
  mezcla con buffer del camino de runs naturales (tipos triviales).
- **Sonda de orden global y regla de ruta:** runs con claves entremezcladas van al quicksort salvo
  que sean pocos. Runs de 63: 0,26 a 0,65 del tiempo anterior.
- **Zonas desordenadas:** 8 o más runs seguidos de menos de 8 elementos se ordenan con el
  quicksort. Cola o bloque al azar del 10%: 0,45 a 0,81.
- **Recorridos vectoriales** en la detección de orden y el conteo de giros (enteros).
- **Selección sin salto** en parity merges, solo gcc y tipos no aritméticos: pares 0,76 a 0,81.
- Regla de ruta y zonas **desactivadas para tipos no triviales**: con strings midieron hasta 1,61.
- Probado y descartado: buffer de 4 a 128 KB y hoja de 32 a 256 (sin diferencia), desenrollado de
  la partición escalar, paso de cabeza sin salto en el galope, selección sin salto para escalares
  (10% peor), mezcla sin salto para runs intercalados que no entran en el buffer (2× peor).
- Los umbrales (8 muestras, 256 muestras, k de 5/4 a 3, zonas de 8/8/16) se ajustaron en una sola
  máquina y sobre varias de las mismas entradas del banco: las filas de datos con estructura de la
  comparación con otros sorts son optimistas.

## Revisiones r2–r6 (18–19 nov 2026): la comparación con Virtual-Memory Powersort

Todo lo que sigue salió de correr SotSort en el harness de los autores de VM-Powersort con la
misma memoria y contar comparaciones y movimientos (`bench/vmpowersort/README.md`, Updates 1–8).
- **r2:** zonas desordenadas (ver arriba).
- **r3:** la densidad de giros se estima con 64 ventanas de 256 pares (n ≥ 2^16); antes se
  pagaba una comparación extra por elemento en entradas con estructura (Observación T de LEMAS).
- **r4:** mezcla por páginas cuando los dos runs superan el buffer (idea de VM-Powersort), en vez
  de rotaciones; regla de elementos grandes (≥ 64 bytes) hacia las mezclas.
- **r5:** runs paginados en el camino de runs naturales (esquema de VM-Powersort); presupuesto de
  memoria honesto (buffer redondeado, buffer propio y tablas); arreglos de generación de código
  (comparador e inserción quedaban fuera de línea con gcc -O2).
- **r6:** (a) diagnóstico: la partición estable binaria cuesta ≥ 1.5 movimientos por elemento y
  nivel más rotaciones que crecen con lg(m/B) (`bench/cost/`); (b) samplesort estable por páginas
  de k vías para entrada casi aleatoria con buffer acotado (mecanismo de Radsort y Logsort; ver
  `docs/IDEA_particion_paginada.md`); (c) tope de 4 particiones malas en QuickEstable
  (Observación P); (d) clasificación por árbol implícito sin saltos (como IPS4o).
- Resultado: con la memoria de VM-Powersort, ninguna de las 12 celdas medidas hace más
  comparaciones (± 0.02) ni más movimientos; con S = 2, 18.96 y 22.10 por elemento contra 27.76 y
  31.65. Peor caso medido con el adversario de McIlroy: ~34 movimientos por elemento.
- Descartado en el camino: rotación por malabarismo (juggling; quedó un rato en main por error y
  se retiró en r4), estimar los runs por muestreo para decidir el paginado (muy grueso), la
  partición de 2 vías por páginas (reemplazada por el samplesort; prototipo en `bench/cost/`).

## Verificación hecha (archivo final)
- `tests/test.cpp` contra `std::stable_sort` (etiquetas para estabilidad): u32/i32/u64/i64, pares,
  registros de 16/40/128 B, strings; modos O1/Bloques/Completo; 1 a 4 hilos; orden por clave;
  15 familias de entradas. 19 506 casos sin fallas (AVX-512); 7 410 con AVX2, sin SIMD y clang;
  ASan+UBSan y TSan limpios.
- `tests/test_extra.cpp`: fallas de reserva inyectadas en cada punto, adversario, comparador que lanza.
- `tests/test_ooo.cpp` y `tests/test_ooo_throw.cpp`: planificador experimental.
- `tests/test_mem.cpp` (r5): el pico de memoria no supera el presupuesto, 600 casos.
- `tests/test_pss.cpp` (r6): samplesort por páginas con registros de 112 bytes y strings; la
  distribución de k vías se verificó además en forma exhaustiva (`bench/cost/paged_samplesort_test.cpp`).
- Un contador temporal confirmó que las pruebas ejecutan las zonas, la regla de ruta y la sonda.
- `tests/adv_quick.cpp`: adversario perezoso del libro contra QuickEstable. Con la línea de v6.1:
  20 a 31 comparaciones por elemento (n = 1000 a 32000). Con `-DSOT_V6_BUG`: 167, 335, 668, 1335
  (aprox. n/6, cuadrático), hasta N9.

## Limitaciones conocidas
- Si el comparador lanza, se pueden perder elementos (quedan objetos movidos). No hay errores de
  memoria. `std::stable_sort` no fue comparado en este punto.
- T debe ser default-constructible y copiable.
- La lectura del cgroup mira solo la raíz del espacio de nombres.
- Sin probar: ARM, Windows, cgroup v1, cgroup con límite real, más de 2 núcleos físicos.
