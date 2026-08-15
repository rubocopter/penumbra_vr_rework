# DIAGNOSIS FINAL — Manos VR (fase de diagnóstico estricto, iteración 2)

Fecha: 2026-08-15 · Estado: DIAGNÓSTICO COMPLETO y CORRECCIÓN APLICADA + VERIFICADA
(ver sección 8)

## 1. Diagnóstico general (causa primaria)

**CAUSA PRIMARIA — Hipótesis B CONFIRMADA (inverse bind incorrecta, por LAYOUT):**
las 17 matrices `Hand_Ctrl-bind_poses` de cada mano están transpuestas respecto a lo que
lee el motor HPL1. El motor lee la traslación en la columna 3 (floats 3/7/11,
`GetTranslation()` = `m[0][3], m[1][3], m[2][3]`, `Matrix.h:236-240`); el archivo la escribe
en la FILA 3 (floats 12/13/14). El motor ve `bindWorld = MatrixInverse(matriz) = Identidad`
para los 17 huesos (`MeshLoaderCollada.cpp:333`), el esqueleto colapsa en el origen y **toda
rotación de cualquier joint pivota en el ORIGEN (muñeca)** — nunca en la articulación.

**Causa secundaria — Hipótesis E:** anatomía de malla con dedos fusionados
(Ring+Middle fundidos para siempre; Index casi sin parte libre) → artefactos residuales
(estiramiento de costuras hasta 74× en grip=1) que quedan tras corregir B.

**Descartadas:** A (bind pose — los joints coinciden con la anatomía), C (skinning —
layout estándar, cómputo `T·B⁻¹` correcto), D (aplicación de transformación — slerp
relativo correcto; los locals previos son identidad por culpa de B).

## 2. Evidencia

### Punto 6 — Verificación numérica definitiva de INV_BIND (script `diagnose_mirror.py`)
Para cada joint (ambas manos): `M = inverse(INV_BIND_MATRIX)`.

| Lectura | errorPosition max | Conclusión |
|---|---|---|
| **Como la lee el motor** (traslación = floats 3/7/11) | **9.7 – 20.3** (M.translation = (0,0,0) en los 17) | **NO pequeño → B NO descartada → B CONFIRMADA** |
| Como la escribió el export (traslación = fila 3) | **0.0000** (los 17) | Las matrices internas son bind matrices estándar consistentes |
| `INV_BIND × M` y `M × INV_BIND` | 3.55e-15 | Identidad a precisión de máquina |

Las matrices son internamente correctas y consistentes; el único defecto es el layout de
almacenamiento (fila 3 vs columna 3) con el que el motor las malinterpreta.

### Punto 1 — Anatomía por eje longitudinal real (script `diagnose_anatomy2.py`)
Proyección de cada vértice sobre el eje J1→J2→J3 de su dedo (ambas manos idénticas):

| Dedo | J2 vs articulación visual A1 | J3 vs A2 | d12 / long.seg1 | d23 / long.seg2 | seg3 |
|---|---|---|---|---|---|
| Little | **0.01** | **0.01** | 5.40 / 5.36 (−0.03) | 3.63 / 3.60 (−0.03) | capa punta (len 0.00) |
| Ring | **0.02** | **0.02** | 6.62 / 6.51 (−0.11) | 4.45 / 4.35 (−0.10) | capa punta |
| Middle | **0.02** | **0.03** | 6.75 / 6.65 (−0.10) | 4.54 / 4.34 (−0.20) | capa punta |
| Index | **0.01** | **0.01** | 5.10 / 5.08 (−0.03) | 3.43 / 3.40 (−0.03) | capa punta |
| Thumb | **0.01** | **0.01** | 2.09 / 2.09 (−0.01) | 1.41 / 1.36 (−0.05) | capa punta |

- **Joint1/2/3 ≈ articulaciones visuales EXACTAS** (≤ 0.03 en todos) → el rig corresponde
  anatómicamente con la geometría (descarta E como causa del pivote).
- Las longitudes geométricas de los segmentos coinciden con las distancias de joints (Δ ≤ 0.20).
- El segmento 3 es una capa de punta (longitud axial 0.00; x 0.4–0.9 de cap).
- Inicio del tubo libre (del análisis de separación): Little x=13.5 (s=4.34 dentro del seg1),
  Ring **nunca** (fundido con Middle hasta la punta x=21.3), Middle **nunca**,
  Index x=18.5 (más allá de J3 — solo queda libre la capa de punta), Thumb x=9.0.

### Punto 2 — Rotación única estricta (script `diagnose_rotate2.py`)
`todas = bind`; se rota UN solo joint +10° (Index1 → Index2 → Index3; luego Middle1/2/3; Ring1).
Eje flex derecha (0,0,+1). Resultados idénticos en estructura para todos los tests:

| Métrica | MOTOR (actual) | REFERENCIA (bind corregido) |
|---|---|---|
| Centro efectivo (ajuste LS) | Sobre el eje por el **ORIGEN**: (0.00, 0.00, ·) | En el **JOINT**: (Jx, Jy, ·) exacto |
| Desplazamiento del punto del joint | **1.6 – 3.5** (¡el joint se mueve!) | **0.000** |
| Desplazamiento del origen | 0.000 | 1.6 – 3.5 |
| Rigidez (residual máx.) | 0.0000 (rotación pura) | 0.0000 |
| Pliegue en el joint | 0.0° (cadena rígida) | 9.8°–24.2° |
| Segmentos 1/2/3 | TODOS se mueven (p.ej. Index1: 2.54/3.13/3.24) | seg1 ≈ 0; seg2/3 giran (0.92/1.50/1.62) |

**La geometría rota alrededor del ORIGEN, NO alrededor del joint esperado.** El bbox de la
región afectada se desplaza igual para Index1, Index2 o Index3 (idéntico movimiento 2.54–3.24):
rotar X1, X2 o X3 produce exactamente la misma deformación — prueba directa de que los tres
joints comparten el mismo pivote (el origen).

### Punto 3 — Palma (script `diagnose_rotate2.py`), Palm +5°
| Región | MOTOR (pivote = origen) | REFERENCIA (pivote = Hand_Root) |
|---|---|---|
| sleeve | **0.000** (no rota) | **0.000** |
| palm | 1.209 | 0.591 |
| dedos (Little…Thumb) | 1.00 – 1.38 | 0.45 – 0.73 |
| estiramiento costura sleeve-palma | **0.555** | **0.292** |
| movimiento relativo palma-dedos | 0.000 (rígido) | 0.000 (rígido) |

La jerarquía `Hand_Root → Palm → dedos` es estructuralmente correcta (los hijos siguen al
padre rígidamente en ambos casos y el sleeve no se deforma). La diferencia es el pivote: en el
motor la palma (y toda la mano) oscila sobre el origen con 2× más estiramiento en la muñeca.

### Punto 4 — Pulgar (script `diagnose_rotate2.py`), Thumb1/2/3 +10° cada uno
Eje (0, 0.891, +0.454):

| Test | MOTOR (pivote = origen) | REFERENCIA (pivote = joint) |
|---|---|---|
| Thumb1 | cadena entera 2.68/2.85/2.85; centro efectivo en el eje por el origen; joint se mueve 2.23 | 0.45/0.63/0.66; centro en Thumb1; joint fijo |
| Thumb2 | 2.68/2.85/2.85 (idéntico al Thumb1) | 0.52/0.30/0.30 |
| Thumb3 | 2.68/2.85/2.85 (idéntico) | 0.74/0.34/0.13 |
| Aislamiento | palma/sleeve/otros dedos: **0.000** (solo vértices con peso de pulgar) | 0.000 |

Los tres tests del pulgar producen el MISMO movimiento en el motor (mismo pivote = origen).
El pulgar no se articula en sus pivots; la palma, el sleeve y el resto de dedos NO se mueven
(aislamiento correcto — el skinning por pesos funciona).

### Punto 5 — Derecha vs izquierda (script `diagnose_mirror.py`)
Por joint (tabla completa en la salida del script): `Right mesh pos` vs `Left mesh pos`:
x y z IDÉNTICAS; y con signo invertido pero con desviaciones de hasta ~0.8 unidades
(p.ej. Thumb1: derecha y=+0.30, izquierda y=+0.48 — espejo exacto sería −0.30).
Vectores distal (joint→child): **paralelos "same"** (dirección +x; la componente y es la que
cambia de signo; ángulo < 15°). Ejes de flexión:

| Cadena | Eje derecha | Eje izquierda | Relación |
|---|---|---|---|
| Little/Ring/Middle/Index | (0,0,+1) | (0,0,−1) | **OPPUESTO (z-flip)** |
| Thumb | (0, 0.891, +0.454) | (0, 0.891, −0.454) | z-flip solo de la componente z |

Conclusión: la dirección de pliegue se espeja correctamente (el código niega z), pero la
geometría de la izquierda NO es el espejo exacto de la derecha (proporciones y distintas,
hasta ±0.8) → el resultado visual en cada mano difiere ligeramente, como se observa en VR.

## 3. Tabla por dedo (punto 10)

| Dedo | Joint1 | Joint2 | Joint3 | Pivot | Bind | Skin | Anatomy |
|---|---|---|---|---|---|---|---|
| Little | OK | OK | OK | **FAIL** (origen) | **FAIL** (transpuesta) | OK | OK (libre 13.5→19) |
| Ring | OK | OK | OK | **FAIL** (origen) | **FAIL** | OK | **FAIL** (fundido con Middle) |
| Middle | OK | OK | OK | **FAIL** (origen) | **FAIL** | OK | **FAIL** (fundido con Ring) |
| Index | OK | OK | OK | **FAIL** (origen) | **FAIL** | OK | **FAIL** (libre solo ~0.3) |
| Thumb | OK | OK | OK | **FAIL** (origen) | **FAIL** | OK | OK (libre siempre, corto) |

Joint1/2/3 = articulación visual vs joint (err ≤ 0.03) — todas OK.
Pivot/Bind = comportamiento del motor con los assets actuales — todas FAIL.

## 4. Síntomas VR → causa concreta (punto 8)

- **"El dedo parece salir desde aquí"** → con pivote en el origen, cada joint del dedo barre
  un arco alrededor de la muñeca: la base (J1, |J|≈9.2 del origen) se desplaza 1.6 y la punta
  2.8–3.5 con solo +10°; el dedo "sale" desde la muñeca porque el centro efectivo del giro es
  el origen, no la articulación (Punto 2: centro efectivo = (0,0,·) para TODOS los tests).
- **"El segmento distal se desplaza porque..."** → en el motor los tres segmentos se desplazan
  idénticamente (2.54/3.13/3.24 para Index1): el tubo completo se inclina sin pliegue (fold 0.0°).
  En la referencia, el distal se desplaza porque el joint (J2/J3) es el pivote: 1.5/1.6 a 10°.
- **"En la mano izquierda ocurre al revés porque..."** → el eje flex se espeja en z
  ((0,0,−1) vs (0,0,+1)) mientras la geometría izquierda NO es el espejo de la derecha
  (y distinta hasta 0.8) → el pliegue es simétrico en dirección pero asimétrico en proporción.
- **"Palma estable"** → Palm y Hand_Root rotan 0° en runtime; y cualquier rotación suya
  pivotaría en el origen (Punto 3: 0.555 de estiramiento en la costura sleeve-palma).
- **"Ángulos antinaturales / abanico"** → 40–60° por articulación, todos pivotando en el
  origen, barren ángulos acumulados de hasta ~165° sobre la muñeca.

## 5. Hipótesis

- **A** (mesh/rig incorrectos en bind pose) = DESCARTADA — los joints coinciden con la anatomía (Punto 1).
- **B** (inverse bind incorrecta) = **PRIMARIA, CONFIRMADA** — errorPosition 9.7–20.3 con la
  lectura del motor (Punto 6); 0.0000 con la lectura del export.
- **C** (skinning incorrecto) = DESCARTADA — layout estándar, `T·B⁻¹` verificado, aislamiento 0.000 (Punto 4).
- **D** (aplicación de transformación) = DESCARTADA — slerp relativo correcto; los locals
  identidad son consecuencia de B.
- **E** (rig ≠ geometría) = SECUNDARIA — articulaciones en su sitio, pero malla fusionada
  (Ring+Middle hasta la punta; Index sin parte libre) → desgarros residuales en grip.

## 6. Corrección recomendada (NO implementada)

Transponer las 17 matrices de `Hand_Ctrl-bind_poses` en `hud_object_hand_rig.dae` y
`hud_object_hand_left_rig.dae`: mover la traslación de la fila 3 (floats 12/13/14) a la
columna 3 (floats 3/7/11), dejando la fila 3 en `0 0 0 1`. Tras el cambio:
`motorBindPos = meshPos` (err ≈ 0) para los 17 joints y el Punto 2 pasa de
"pliegue 0.0°, pivote = origen" a "pliegue 9.8°–24.2°, pivote = joint".

Nada más cambia: pesos, ejes, ángulos, jerarquía y animación quedan intactos.
Efecto residual esperado: los desgarros de las costuras fusionadas (E) permanecen.

## 7. Archivos de evidencia

- `scripts/diagnose_rig.py` — resolución INV_BIND, tabla per-joint, jerarquía, producto, espejo.
- `scripts/diagnose_anatomy.py` — separación de tubos, regiones, ambas manos.
- `scripts/diagnose_anatomy2.py` — anatomía por eje longitudinal (articulaciones vs joints).
- `scripts/diagnose_rotate.py` — test de rotación única v1 (pliegues motor/referencia).
- `scripts/diagnose_rotate2.py` — tests estrictos Index/Middle/Ring + palma + pulgar con
  centro efectivo y aislamiento.
- `scripts/diagnose_mirror.py` — tabla derecha/izquierda + verificación numérica INV_BIND.
- `scripts/diagnose_grip.py` — skinning grip=1 (pliegues, edge-stretch 74×, regiones).
- `scripts/render_hand.py` + `render_0/1/2.png` — renders bind vs grip=1 (triángulos reales).
- `scripts/DIAGNOSIS.md` — informe del hallazgo (iteración 1).

**Restricciones respetadas:** solo scripts en `scripts/`; sin escrituras en `.dae`/`.hud`/`.cpp`/`.h`/
`actions.json`/`bindings`; sin build/package/deploy. **Se detiene aquí: esperando aprobación explícita.**

## 8. Resolución (2026-08-15) — corrección aplicada y verificada

**Aplicado:** `scripts/fix_rig_bind_layout.py` transpuso las 17 matrices bind de ambas manos
(traslación floats 12/13/14 → 3/7/11; fila 3 = `0 0 0 1`). Único cambio en los `.dae` es la
línea del `float_array` (diff de 1 línea por archivo); sin `ns0:`. Backups:
`.penumbravr/backup/*.dae.pre-bind-layout-fix` (los `.pre-weightfix` intactos).

**Verificado** (`scripts/verify_bind_fix.py`, modelo exacto del motor `Node3D::UpdateMatrix`,
local = (R, P_j), + Kabsch/ajuste de eje):

| Parte | Resultado (ambas manos) |
|---|---|
| A: `motorBindPos` vs `meshPos` (17 joints) | err **0.0000**; `IB×M = M×IB = I` (0.00e+00) → **B descartada** |
| B1: rotación única +10° (Index1/2/3, Middle1/2/3, Ring1, Thumb1/2/3) | eje por el **joint** (d 0.0000), joint fijo (0.0000), segmento rota 10.00°, proximal y palma 0.0000 — 10/10 OK × 2 |
| B2: grip=1 (ángulos/ejes de `PlayerHands.cpp`) | palma/sleeve 0.0000; rotaciones acumuladas exactas 60/120/165, 55/110/150, 0, 40/75/75; Thumb1 fijo; Thumb2/3 jerárquicos — OK |
| Renders (`render_hand.py`) | `render_0/1/2.png` regenerados con el rig corregido y los ángulos/ejes reales |

Build Debug y Release OK (`check-project.ps1` OK), `package.ps1` y `deploy.ps1` OK; hashes
SHA-256 de ambos `.dae` idénticos en paquete y en `redist` de Steam
(`94C4DD1C…` derecha, `4E1666E8…` izquierda).

**Pendiente conocido (hipótesis E):** Ring+Middle fundidos e Index sin parte libre pueden
mantener artefactos menores de costura en grip=1; el pivote en articulación ya es correcto.