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

## Verificación hecha
- `test.cpp` contra `std::stable_sort` (etiquetas para estabilidad): u32/i32/u64/i64, pares,
  registros de 16/40/128 B, strings; modos O1/Bloques/Completo; 1 a 4 hilos; orden por clave.
  24 390 casos sin fallas (AVX-512); 6 586 con AVX2, sin SIMD, clang; ASan+UBSan y TSan limpios.
- `test_extra.cpp`: fallas de reserva inyectadas en cada punto, adversario, comparador que lanza.
- Cobertura de líneas (gcov, antes de `test_extra`): 638 de 667; lo no cubierto era el respaldo,
  la caída escalonada y el cgroup v1. `test_extra` ejercita los dos primeros; no volví a medir.
- `adv_quick.cpp`: adversario perezoso del libro contra QuickEstable. Con la línea de v6.1:
  20 a 31 comparaciones por elemento (n = 1000 a 32000). Con `-DSOT_V6_BUG`: 167, 335, 668, 1335
  (aprox. n/6, cuadrático), hasta N9.

## Limitaciones conocidas
- Si el comparador lanza, se pueden perder elementos (quedan objetos movidos). No hay errores de
  memoria. `std::stable_sort` no fue comparado en este punto.
- T debe ser default-constructible y copiable.
- La lectura del cgroup mira solo la raíz del espacio de nombres.
- Sin probar: ARM, Windows, cgroup v1, cgroup con límite real, más de 2 núcleos físicos.
