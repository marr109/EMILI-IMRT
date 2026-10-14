# =============================================================================
#  FMO (Fluence Map Optimization) con penalización por órgano
# =============================================================================


# ----------------------------------------------------------------------------
#  Tamaños de la instancia
# ----------------------------------------------------------------------------
param n_ptv integer >= 0;       # número total de vóxeles (filas) del PTV
param n_oar integer >= 0;       # número total de vóxeles (filas) de todos los OAR
param n_organs integer >= 1;    # número de órganos en riesgo (ej. 2: vejiga y recto)


# ----------------------------------------------------------------------------
#  Conjuntos de índices
# ----------------------------------------------------------------------------
set DIMLETS;                         # beamlets activos (solo los de los K ángulos elegidos)
set PTV_B := 0 .. n_ptv - 1;         # vóxeles del PTV
set OAR_B := 0 .. n_oar - 1;         # vóxeles de los OAR
set ORGANS := 0 .. n_organs - 1;     # órganos en riesgo (0 = primero, 1 = segundo, ...)


# ----------------------------------------------------------------------------
#  Parámetros de la penalización
# ----------------------------------------------------------------------------
param max_intensity > 0;             # intensidad máxima permitida por beamlet
param w_under >= 0;                  # peso de la subdosis en el PTV
param w_ptv_over >= 0 default 0;     # peso de los puntos calientes en el PTV
                                     # (0 = no se penalizan)

# A qué órgano pertenece cada vóxel de OAR (un índice de ORGANS por vóxel).
param oar_organ {OAR_B} integer >= 0, < n_organs;

# Peso de sobredosis PROPIO de cada órgano. Es el cambio central de este modelo:
# subir el peso de un órgano hace que el optimizador lo proteja más, a costa de
# la cobertura del tumor y de los demás órganos.
param w_over_organ {ORGANS} >= 0;


# ----------------------------------------------------------------------------
#  Normalización por volumen (opcional no incluida en fmo.mod)
# ----------------------------------------------------------------------------
#  normalize = 0 : cada término suma los cuadrados de las holguras (escala de
#                  fmo.mod). Un órgano con más vóxeles pesa más en el objetivo.
#
#  normalize = 1 : cada término es el error cuadrático MEDIO de su estructura
#                  (suma dividida por su número de vóxeles). Así los pesos
#                  comparan estructuras en igualdad de condiciones, sin importar
#                  su tamaño. El objetivo cambia de escala, por lo que no es
#                  comparable con corridas hechas con normalize = 0.

param normalize binary default 0;
param organ_size {o in ORGANS} := card {b in OAR_B: oar_organ[b] = o};   # vóxeles por órgano
param scale_ptv := if normalize = 1 and n_ptv > 0 then 1 / n_ptv else 1;
param scale_organ {o in ORGANS} :=
    if normalize = 1 and organ_size[o] > 0 then 1 / organ_size[o] else 1;


# ----------------------------------------------------------------------------
#  Límites de dosis (por vóxel)
# ----------------------------------------------------------------------------
param dmin {PTV_B} >= 0;                          # dosis mínima exigida al PTV
param dmax {OAR_B} >= 0;                          # dosis máxima tolerada en cada vóxel de OAR
param dmax_ptv {PTV_B} >= 0 default Infinity;     # techo de dosis en el PTV
                                                  # (Infinity = sin techo)


# ----------------------------------------------------------------------------
#  Matriz de influencia (dosis por unidad de intensidad), en forma dispersa
# ----------------------------------------------------------------------------
#  d_ptv[b,j] = dosis que el beamlet j entrega al vóxel b del PTV.
#  d_oar[b,j] = dosis que el beamlet j entrega al vóxel b de un OAR.
#  Solo se declaran los pares (vóxel, beamlet) con dosis distinta de cero.
set PTV_DOSE within {PTV_B, DIMLETS};
set OAR_DOSE within {OAR_B, DIMLETS};
param d_ptv {PTV_DOSE} >= 0;
param d_oar {OAR_DOSE} >= 0;


# ----------------------------------------------------------------------------
#  Variables de decisión
# ----------------------------------------------------------------------------

var x {DIMLETS} >= 0, <= max_intensity;   # intensidad de cada beamlet (la decisión real)

#  Las tres siguientes son HOLGURAS: se deducen de x y miden cuánto se viola
#  cada límite. En el óptimo valen max(0, violación) de su vóxel; no son
#  decisiones independientes, sirven para convertir los límites en penalizaciones.

var u {PTV_B} >= 0;    # subdosis del vóxel b del PTV      (cuánto falta para dmin)
var v {OAR_B} >= 0;    # sobredosis del vóxel b de un OAR  (cuánto excede dmax)
var w {PTV_B} >= 0;    # punto caliente del vóxel b del PTV (cuánto excede dmax_ptv)


# ----------------------------------------------------------------------------
#  Función objetivo
# ----------------------------------------------------------------------------
#  Suma de tres penalizaciones cuadrática. El peso de cada
#  vóxel de OAR depende del órgano al que pertenece: w_over_organ[oar_organ[b]].
minimize fmo_objective:
    w_under * scale_ptv * sum {b in PTV_B} u[b]^2
  + sum {b in OAR_B} w_over_organ[oar_organ[b]] * scale_organ[oar_organ[b]] * v[b]^2
  + w_ptv_over * scale_ptv * sum {b in PTV_B} w[b]^2;


# ----------------------------------------------------------------------------
#  Restricciones
# ----------------------------------------------------------------------------

#  La dosis de un vóxel es la suma, sobre los beamlets, de (dosis por unidad de
#  intensidad) * (intensidad):  D_b = SUMA_j d[b,j] * x[j].

# Piso del PTV: dosis + holgura >= dmin.
# Si el vóxel recibe menos que dmin, u[b] absorbe la diferencia (y se penaliza);
# si cumple, u[b] = 0 y no cuesta nada.

subject to ptv_floor {b in PTV_B}:
    sum {(b,j) in PTV_DOSE} d_ptv[b,j] * x[j] + u[b] >= dmin[b];

# Techo de OAR: dosis - holgura <= dmax.
# Si el vóxel supera dmax, v[b] mide el exceso (y se penaliza con el peso de su
# órgano); si cumple, v[b] = 0.
subject to oar_ceiling {b in OAR_B}:
    sum {(b,j) in OAR_DOSE} d_oar[b,j] * x[j] - v[b] <= dmax[b];

# Techo del PTV (homogeneidad): dosis - holgura <= dmax_ptv.
# Evita puntos calientes dentro del tumor. Con w_ptv_over = 0 esta fila no
# cuesta nada, por lo que en la práctica queda desactivada.
subject to ptv_ceiling {b in PTV_B}:
    sum {(b,j) in PTV_DOSE} d_ptv[b,j] * x[j] - w[b] <= dmax_ptv[b];
