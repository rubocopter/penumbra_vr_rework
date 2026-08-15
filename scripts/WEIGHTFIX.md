# Corrección de pesos del rig de manos (hud_object_hand_rig.dae / hud_object_hand_left_rig.dae)

## Problema
Los pesos del skin estaban asignados a los huesos **distales** (X2/X3) en vez de los **proximales** (X1):
- Tubos de los dedos pesaban a Little3/Index3/Ring2/Middle2 (Little1, Index1, Middle1, Ring1 = 0%).
- La palma estaba atada al pulgar (Thumb2/Thumb3).

Consecuencia visual: al cerrar el puño (grip), las falanges X1 rotaban sin arrastrar vértices
(dedos extendidos) y los tubos colgaban de los pivotes X2/X3 (pliegue en "V" en el pivote equivocado,
dedos separados/deformes). La palma además se retorcía con el pulgar.

## Criterio de reasignación (scripts/fix_rig_weights.py)
Asignación dura 100% a un solo hueso por vértice, calculada a partir de la geometría bind del propio .dae:

1. **Manga (x < 9.0)** → `Hand_Root` (sin cambios; ya era correcto).
2. **Dedos** (Little, Ring, Middle, Index, Thumb): se recorre la polilínea de joints
   (X1→X2→X3 de cada cadena) y se mide la distancia mínima de cada vértice a cada polilínea:
   - distancia ≤ R de la cadena más cercana → asignado a la cadena, por tramo:
     - vértice proyectado entre X1 y X2 → `X1` (la flexión ocurre en el nudillo),
     - entre X2 y X3 → `X2`,
     - más allá de X3 → `X3`.
   - **R por cadena** = percentil 50 de las distancias de un corte transversal del tramo
     proximal-medio (anillo del tubo) + 0.45. Valores resultantes:
     Little 2.37, Ring 2.50, Middle 2.32, Index 2.30, Thumb 1.90.
     Esto captura la pared del tubo (±0.4 alrededor del radio) pero deja fuera las superficies
     planas de la palma/red interdigital (distancia ≈ 2.6–2.9), que van a `Palm`.
3. **Todo lo demás (x ≥ 9)** → `Palm` (superficie palmar, red entre nudillos, dorso entre dedos).

Las matrices bind de COLLADA se leen en orden **column-major** (la traslación está en los
índices 12:15 de la fila 3, no en (3,7,11)).

## Verificación
- `check_weights2.py` tras aplicar: segmentos proximales dominados por X1 (Little1 88.7%,
  Ring1 100%, Middle1 83.8%, Index1 92%), medios por X2 (72–82%), puntas por X3, manga 100% Hand_Root.
- `verify_skin.py` (simulación de la pose de grip): la base del tubo sube hacia la palma (+0.90 en Y),
  las puntas se pliegan hacia la palma (+2.37), la manga queda rígida (desplazamiento 0.0000).
- Geometría y joints idénticos al backup (solo cambia la tabla de pesos).

## Reproducir
1. `python scripts\fix_rig_weights.py --dry-run` (resumen, sin escribir).
2. `python scripts\fix_rig_weights.py --apply` (reescribe ambos .dae).
3. Originales respaldados en `.penumbravr\backup\*.dae.pre-weightfix`.