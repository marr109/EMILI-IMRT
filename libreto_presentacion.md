# Libreto conversacional — Reunión con Guillermo

Guión pensado como conversación, no como exposición formal.

---

## Slide 1 — Portada

"Guillermo, gracias por el tiempo. Quería ponerte al día con todo lo que he avanzado, porque hubo bastantes cosas que tuve que corregir en el camino y quiero que entiendas bien en qué estado está el sistema antes de hablar de resultados."

---

## Slide 2 — Agenda

"Te voy a contar cinco cosas: primero cómo está armado el sistema, después cuatro modificaciones que tuve que hacer porque había bugs que afectaban los resultados, luego cómo calibré los parámetros con irace, los resultados que tengo hasta ahora, y finalmente lo que queda pendiente. La parte de las modificaciones es probablemente la más importante."

---

## Slide 3 — Arquitectura del sistema

"La idea central es que el problema tiene dos niveles. El nivel de arriba es el BAO — decidir qué ángulos de gantry usar. Para eso uso un ILS, una búsqueda local iterada, que va probando combinaciones de ángulos y queda con la que minimiza la función objetivo.

El problema es que para saber si una combinación de ángulos es buena, tenés que resolver el FMO, que es optimizar las intensidades de los beamlets dados esos ángulos. Y eso no es trivial — cada evaluación llama a OSQP, un solver exacto de programación cuadrática, así que la solución es óptima pero cuesta tiempo.

La instancia clínica es un caso real de próstata del conjunto CORT: 36 ángulos candidatos, más de 20 mil vóxeles, y los órganos en riesgo son vejiga y recto."

---

## Slide 4 — Modificaciones al modelo

"Acá está lo más importante de la reunión. Encontré cuatro bugs, y algunos afectaban seriamente los resultados sin que fuera obvio.

El primero fue en el ILS mismo. El loop externo — el que controla los reinicios — estaba usando el mismo criterio de terminación que el loop interno. Entonces el ILS terminaba después del primer reinicio, siempre, sin importar cuántos reinicios configurabas. Eso lo corregí con un null-check.

El segundo fue en el script que conecta irace con EMILI. Había una línea que consumía el primer argumento del script para guardarlo en una variable llamada BOUND, lo que desplazaba todos los parámetros del ILS una posición. irace estaba entrenando con configuraciones completamente distintas a las que creía que estaba probando. Lo peor es que no daba error — simplemente calibraba mal en silencio.

El tercero fue el más crítico. Todas las instancias reales tenían max_intensity igual a 10. El problema es que las dose rates de los datos CORT son del orden de 10 elevado a menos 6, entonces con ese límite OSQP entregaba intensidades tan bajas que la dosis en todos los vóxeles era prácticamente cero. La función objetivo quedaba en valores alrededor de 31 millones, que no tiene ningún sentido clínico. Lo corregí a 15.000 en todas las instancias reales.

El cuarto fue que faltaba un término en la función objetivo: la penalización de sobredosis en el PTV. Sin ese peso, el FMO no tenía incentivo para evitar que el tumor recibiera dosis demasiado alta, lo que tampoco es deseable clínicamente. Lo añadí."

---

## Slide 5 — Instancias y submuestreo

"Para usar irace necesitaba instancias más pequeñas. Con la instancia completa, cada evaluación tarda entre 32 y 64 minutos, lo que hace imposible correr 52 experimentos en un presupuesto razonable.

Lo que hice fue generar instancias submuestreadas: tomé una muestra aleatoria de los vóxeles originales, bajando de 20 mil a 2.500, pero manteniendo las proporciones relativas de PTV, vejiga y recto. Generé dos versiones con semillas distintas para tener más variabilidad.

El flujo de trabajo quedó así: desarrollo rápido con la instancia sintética que tarda 30 segundos, calibración de irace con las instancias submuestreadas, y validación clínica definitiva en la instancia completa."

---

## Slide 6 — Calibración con irace

"irace busca automáticamente la mejor configuración del ILS probando muchas combinaciones y comparando estadísticamente sus resultados. Los parámetros que exploré fueron cuatro: si el vecindario se explora con best improvement o first improvement, cómo se inicializa la solución, cuántos ángulos se intercambian en cada perturbación, y cuántos reinicios hace el ILS.

El presupuesto fue de 200 mil segundos CPU con 4 cores — unas 14 horas en total, lo que da aproximadamente 52 experimentos."

---

## Slide 7 — Configuración ganadora

"La configuración que irace seleccionó fue: best improvement, inicializar con los primeros K ángulos, un swap en la perturbación, y 3 reinicios.

Pero acá hay un problema importante que ya mencioné: esta calibración se hizo con max_intensity igual a 10, que es el valor incorrecto. Eso significa que irace estuvo optimizando sobre runs donde el FMO no funcionaba bien. La configuración puede tener sentido estructuralmente, pero no puedo confiar en ella hasta re-ejecutar irace con el valor correcto. Eso es lo primero que está pendiente."

---

## Slide 8 — Resultados

"Los resultados que tengo son con la configuración ganadora sobre la instancia submuestreada, K igual a 4.

Los ángulos seleccionados fueron 0°, 100°, 130° y 220°. La función objetivo quedó en 8.329, que está en el rango de plan con violaciones menores. El tiempo fue de unos 32 minutos.

En términos clínicos: el D95 del PTV llegó a 62.3 Gy cuando la meta es 64.6, y la cobertura V95% quedó en 88.6% en lugar del 100% ideal. La vejiga y el recto tienen violaciones puntuales de dosis máxima — 70 y 60 Gy respectivamente cuando el límite es 50.

Lo que hay que tener en cuenta es que estas violaciones son puntuales, en uno o dos vóxeles, y que la instancia submuestreada tiende a subestimar la cobertura real del PTV. Por eso la validación en la instancia completa es importante antes de sacar conclusiones."

---

## Slide 9 — Calidad del plan

"Para cerrar los resultados, los índices estándar de calidad. El CI mide conformidad — qué tan bien el plan se ajusta al PTV. El HI mide homogeneidad dentro del tumor. El V95% es la fracción del PTV que recibe al menos el 95% de la dosis prescrita.

Con el D95 en 62.3 Gy el plan tiene cobertura subóptima, pero la estructura es clínicamente válida. Espero que al validar en la instancia completa, y con irace re-calibrado, los resultados mejoren."

---

## Slide 10 — Próximos pasos

"Tengo cuatro cosas concretas para hacer. Lo más urgente es re-ejecutar irace con max_intensity correcto — unas 14 horas. Luego validar la configuración ganadora en la instancia completa, que toma unas 5 horas y media. Con eso puedo hacer el análisis definitivo de cobertura del PTV y restricciones en OARs.

Si los resultados con ILS no son suficientemente buenos después de eso, podría explorar variantes como VNS o búsqueda tabú en el nivel BAO.

Eso es todo de mi parte. Me interesa saber qué te parece el estado actual, si los resultados en la instancia submuestreada te parecen razonables dado que la calibración está pendiente, y si hay algún índice clínico que quieras que incorpore al análisis."

---

> **Tiempo estimado:** 15–20 minutos
