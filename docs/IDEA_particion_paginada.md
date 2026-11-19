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

## 3. Estado de la literatura (búsqueda del 19 nov 2026, no exhaustiva)

- IPS4o y su versión de revista ("Engineering In-place (Shared-memory) Sorting Algorithms",
  TOPC 2022): clasificación en bloques de buffer, volcado en bloques ya leídos y permutación
  de bloques. **No preserva el orden de los bloques dentro de un bucket**, así que no es
  estable; la revista no describe una variante estable de la permutación de bloques.
- Katajainen y Pasanen (BIT 1992): partición estable en O(n) tiempo y O(1) espacio.
  Resultado teórico; constantes grandes, no orientado a movimientos con buffer.
- glidesort y el quicksort estable de la biblioteca estándar de Rust: partición estable a
  un scratch de tamaño proporcional a n (n/8 o más); no paginan.
- VM-Powersort: páginas para **mezclas**, no para particiones.
- No encontré una partición estable paginada con un bit por página. Eso **no** prueba que
  sea nueva: falta buscar en literatura de external memory / stable distribution sort y
  preguntar.

## 4. Próximos pasos

1. Escribir la demostración de la afirmación 3 (invariante de ranuras libres).
2. Prototipo aislado de la partición paginada, con el mismo tipo contador, y comparar la
   curva de movimientos con la tabla de la sección 1.
3. Derivar la constante c de la partición actual.
4. Solo después: integrar al quicksort y medir conteos en el harness de VM-Powersort.
