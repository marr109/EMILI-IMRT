# Generadores de solución inicial y vecindarios — por explorar

Lista de candidatos a incorporar más adelante, más allá de lo que ya está
implementado y en uso (`FirstKAnglesInit`, `RandomKAnglesInit` para solución
inicial; `AngleShiftNeighborhood`/`AngleMultiShiftNeighborhood` para
vecindario). Ninguno de estos está implementado todavía salvo que se indique
lo contrario.

## Ya programado pero sin usar en experimentos

- **`AngleSwapNeighborhood`** (`imrt/imrt_bao.h`/`.cpp`) — reemplaza un
  ángulo activo por uno inactivo, en vez de desplazar ±step. Nunca se corrió
  sistemáticamente en los experimentos de First/Best/ILS de esta carpeta
  (todos usaron `nangshift`/shift). Candidato inmediato para ser el segundo
  vecindario de un VNS, porque no requiere código nuevo.

## Generadores de solución inicial

- **Equiespaciado**: K ángulos repartidos uniformemente en 360° (ej. K=4 →
  0°,90°,180°,270°), con un offset aleatorio para no repetir siempre el
  mismo punto de partida. Evita que el punto de partida quede agrupado por
  azar, cosa que sí le puede pasar a `RandomKAnglesInit`.
- **Muestreo por sectores**: dividir 360° en K sectores iguales y elegir un
  ángulo al azar dentro de cada sector. Combina la aleatoriedad de RandomK
  con la garantía de dispersión del equiespaciado.
- **Construcción greedy**: arrancar vacío y agregar ángulos de a uno, cada
  vez eligiendo el que más mejora el objetivo FMO. La lógica greedy ya existe
  en `GreedyAnglesPerturbation` (se usa como perturbación de Iterated
  Greedy, destruye y reconstruye) — reutilizarla como generador de solución
  inicial (sin la fase de destrucción) sería una extensión directa.

## Vecindarios / movimientos nuevos

- **Swap múltiple (2+ ángulos a la vez)**: análogo a un "double bridge" —
  cambia dos (o más) ángulos activos por dos inactivos en un solo
  movimiento, en vez de uno a la vez como hace `AngleSwapNeighborhood` hoy.
- **Rotación rígida del conjunto**: desplazar los K ángulos activos todos
  juntos por el mismo delta, manteniendo la forma relativa del arreglo de
  haces. Estructuralmente distinto a mover un solo ángulo — cambia dónde
  apunta el conjunto completo sin cambiar su geometría interna.
- **Destroy-rebuild parcial como vecindario explorable**: variante LNS de lo
  que ya hace `GreedyAnglesPerturbation`, pero expuesta como vecindario
  navegable (First/Best) en vez de solo como perturbación de un solo paso.

## Por qué importa ahora

VNS necesita más de un vecindario para tener sentido (quedó pendiente de la
reunión con Leslie, junto con definir el criterio de Tabu Search — ver
[[project_leslie_localsearch_review]]). `AngleSwapNeighborhood` ya
programado y sin usar es el camino más barato para arrancar esa
comparación sin escribir código nuevo primero.
