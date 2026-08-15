# BINDFIX — Corrección del layout de las inverse bind matrices

Fecha: 2026-08-15 · Estado: APLICADA y VERIFICADA (ambas manos, desplegada a Steam)

## Causa raíz

Las 17 matrices `Hand_Ctrl-bind_poses` de cada mano (`hud_object_hand_rig.dae` y
`hud_object_hand_left_rig.dae`) están transpuestas respecto a lo que lee el motor HPL1:

- El motor lee la traslación en la **columna 3**: `GetTranslation()` = `m[0][3], m[1][3], m[2][3]`
  (`HPL1Engine/include/math/Matrix.h:236-240`), y construye
  `bindWorld = MatrixInverse(INV_BIND_MATRIX)` (`MeshLoaderCollada.cpp:333`).
- El export las escribió en la **fila 3** (floats 12/13/14).

Consecuencia: `bindWorld = Identidad` para los 17 huesos → todo joint pivota en el origen
(muñeca). Ver `scripts/DIAGNOSIS_FINAL.md` (hipótesis B).

## La corrección

En las 17 matrices (solo el `float_array` de `Hand_Ctrl-bind_poses`, 272 floats):

```
new[3]  = old[12];  new[7]  = old[13];  new[11] = old[14]   # traslación -> columna 3
new[12] = new[13] = new[14] = 0;        new[15] = 1          # fila 3 = 0 0 0 1
```

Las rotaciones (identidad), la jerarquía, los pesos, los ejes, los ángulos y el resto del
XML quedan byte a byte idénticos. La edición es textual (solo el contenido del `float_array`),
por lo que el namespace COLLADA por defecto se conserva (sin `ns0:`).

## Uso

```
python scripts/fix_rig_bind_layout.py --dry-run   # valida y muestra before/after
python scripts/fix_rig_bind_layout.py --apply     # backup + escribe
```

El script verifica la estructura (count=272, fila 3 = `0 0 0 1`, traslación presente) y es
idempotente: sobre un archivo ya corregido informa "layout ALREADY FIXED" y no toca nada.

Backups (nunca se tocan los `.pre-weightfix`):

- `.penumbravr/backup/hud_object_hand_rig.dae.pre-bind-layout-fix`
- `.penumbravr/backup/hud_object_hand_left_rig.dae.pre-bind-layout-fix`

## Verificación (resumen; script `scripts/verify_bind_fix.py`)

- **Parte A (matemática, 17 joints × 2 manos):** `motorBindPos = meshPos` exacto (err 0.0000);
  `INV_BIND × M = M × INV_BIND = I` (0.00e+00); hipótesis B DESCARTADA.
- **Parte B1 (rotación única +10°, ejes runtime):** el eje de rotación pasa por el joint
  (distancia 0.0000), el joint no se mueve (0.0000), el segmento rota exactamente 10.00°,
  segmento proximal y palma/sleeve estables (0.0000) — los 10 tests × 2 manos.
- **Parte B2 (grip=1 con los ángulos/ejes de `PlayerHands.cpp`):** palma/sleeve sin
  desplazamiento (0.0000); rotaciones por joint acumuladas exactas
  (Middle/Ring 60/120/165, Little 55/110/150, Index 0, Thumb 40/75/75); Thumb1 fijo;
  Thumb2/3 siguen la cadena jerárquicamente. Sin rotación global.
- Renders regenerados (`scripts/render_hand.py` → `render_0/1/2.png`) con el rig corregido
  y los ángulos/ejes reales del runtime.

## Entregables

- `scripts/fix_rig_bind_layout.py` — la corrección.
- `scripts/verify_bind_fix.py` — verificación post-fix (Parte A/B, ambas manos).
- `data/models/hud_objects/hud_object_hand_rig.dae`, `hud_object_hand_left_rig.dae` — corregidos.
- Build Debug + Release OK, `package.ps1` y `deploy.ps1` OK; hashes SHA-256 idénticos en
  paquete y en `redist` de Steam (94C4DD1C… / 4E1666E8…).

Problema residual conocido (fuera de alcance): hipótesis E — Ring+Middle fusionados e Index
sin parte libre pueden dejar artefactos menores de costura en grip; el pivote ya es correcto.