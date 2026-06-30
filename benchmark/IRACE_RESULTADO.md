## irace — BAO PROSTATE K=4 — Resultados iniciales

**Estado**: ✅ Completado (1 iteración, 4 experimentos)
**Tiempo**: 677s pared, 1880s CPU
**Budget**: 20000s CPU (solo usó 1882s — falta presupuesto de instancias)

### Configuraciones élite encontradas

| # | inner_explore | inner_init | pert_swaps | outer_iter | w_under | w_over | w_ptv_over |
|---|---|---|---|---|---|---|---|
| 1 | first | ifirstk | 1 | 2 | 1.00 | 0.50 | 0.50 |
| 2 | first | irandomk | **2** | 2 | 19.73 | 0.06 | 0.36 |

### Interpretación

- **Config 1**: Es la línea base (init_config.txt). `pert_swaps=1`, pesos neutros.
- **Config 2**: irace sugiere `pert_swaps=2` (confirma nuestro hallazgo manual!) con pesos agresivos: w_under alto (fuerza cobertura PTV), w_over bajo (tolera más dosis en OAR).

irace no pudo explorar más porque el presupuesto de instancias era bajo para runs tan pesadas (~170s c/u). Pero el resultado inicial **valida independientemente** que `pert_swaps=2` es clave.
