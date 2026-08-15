"""Fix the COLLADA inverse bind matrix layout of the VR hand rigs.

Root cause (see scripts/DIAGNOSIS_FINAL.md, Hypothesis B): HPL1 reads a
matrix's translation from floats 3/7/11 (GetTranslation() = m[0][3], m[1][3],
m[2][3], Matrix.h), but the FBX export wrote the translation in the LAST ROW
(floats 12/13/14). The engine therefore sees bindWorld = identity for every
bone and every joint pivots at the hand origin.

This script transposes the translation of the 17 Hand_Ctrl-bind_poses matrices
of each hand:
    new[3]  = old[12];  new[7]  = old[13];  new[11] = old[14];
    new[12] = new[13] = new[14] = 0;  new[15] = 1
Rotations are untouched. Everything else in the file is byte-identical.

The edit is text-based (only the <float_array> content of the bind_poses
source is rewritten) so the COLLADA default namespace and all other XML are
preserved exactly; no ns0: prefix is introduced.

Backups: .penumbravr/backup/<name>.dae.pre-bind-layout-fix (the pre-weightfix
backups are never touched).

Usage:
  python fix_rig_bind_layout.py --dry-run
  python fix_rig_bind_layout.py --apply
"""

import argparse
import os
import re
import shutil
import sys

FILES = [
    'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae',
    'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae',
]
BACKUP_DIR = 'E:/penumbra_vr-master/.penumbravr/backup'
EXPECTED = 17 * 16  # 272


def find_bind_pose_arrays(text):
    """Return list of (source_id, count, tokens) for every INV_BIND_MATRIX
    source referenced by a controller skin."""
    outs = []
    for m in re.finditer(r'<input[^>]*semantic="INV_BIND_MATRIX"[^>]*source="([^"]+)"', text):
        sid = m.group(1)[1:]
        fa = re.search(r'(<float_array[^>]*id="%s(?:-array)?"[^>]*count="(\d+)"[^>]*>)(.*?)(</float_array>)' %
                       re.escape(sid), text, re.S)
        if fa is None:
            raise SystemExit('ERROR: no float_array found for source #%s' % sid)
        count = int(fa.group(2))
        tokens = fa.group(3).split()
        outs.append((sid, count, tokens, fa.start(), fa.end(), fa.group(1), fa.group(4)))
    return outs


def structure_check(sid, count, tokens):
    errs = []
    if count != EXPECTED:
        errs.append('count=%d, expected %d' % (count, EXPECTED))
    if len(tokens) != EXPECTED:
        errs.append('%d floats parsed, expected %d' % (len(tokens), EXPECTED))
    for k in range(0, len(tokens), 16):
        m = tokens[k:k + 16]
        try:
            col3 = [float(m[3]), float(m[7]), float(m[11])]
            last_row = [float(m[12]), float(m[13]), float(m[14]), float(m[15])]
        except ValueError as e:
            errs.append('matrix %d: non-numeric token: %s' % (k // 16, e))
            continue
        if max(abs(v) for v in col3) > 1e-6:
            errs.append('matrix %d: translation already in column 3 (%s) - '
                        'file appears already fixed or layout unexpected' %
                        (k // 16, col3))
        if abs(last_row[3] - 1.0) > 1e-6:
            errs.append('matrix %d: last float is %s, expected 1' % (k // 16, m[15]))
        if max(abs(v) for v in last_row[:3]) < 1e-6:
            errs.append('matrix %d: no translation in last row - nothing to move' % (k // 16))
    return errs


def build_fixed_tokens(tokens):
    out = list(tokens)  # preserve original token strings
    for k in range(0, len(tokens), 16):
        out[k + 3] = tokens[k + 12]
        out[k + 7] = tokens[k + 13]
        out[k + 11] = tokens[k + 14]
        out[k + 12] = '0'
        out[k + 13] = '0'
        out[k + 14] = '0'
        out[k + 15] = '1'
    return out


def process(path, apply):
    text = open(path, encoding='utf-8').read()
    if 'ns0:' in text:
        raise SystemExit('ERROR: %s already contains ns0: - aborting' % path)
    arrays = find_bind_pose_arrays(text)
    if not arrays:
        raise SystemExit('ERROR: no INV_BIND_MATRIX input found in %s' % path)
    print('== %s' % os.path.basename(path))
    for sid, count, tokens, start, end, open_tag, close_tag in arrays:
        errs = structure_check(sid, count, tokens)
        if errs:
            already_fixed = all(
                max(abs(float(m[3])), abs(float(m[7])), abs(float(m[11]))) > 1e-6
                and max(abs(float(m[12])), abs(float(m[13])), abs(float(m[14]))) < 1e-6
                and abs(float(m[15]) - 1.0) < 1e-6
                for m in (tokens[k:k + 16] for k in range(0, len(tokens), 16)))
            if already_fixed:
                print('  source #%s: layout ALREADY FIXED (translation in column 3) - '
                      'nothing to do' % sid)
                continue
            print('  source #%s: STRUCTURAL FAILURE - not modifying:' % sid)
            for e in errs:
                print('    - %s' % e)
            return 1
        fixed = build_fixed_tokens(tokens)
        print('  source #%s: %d matrices x 16 floats (count=%d) - structure OK' %
              (sid, count // 16, count))
        k = 0
        print('    first matrix BEFORE: %s' % ' '.join(tokens[k:k + 16]))
        print('    first matrix AFTER : %s' % ' '.join(fixed[k:k + 16]))
        changed = sum(1 for i in range(0, len(tokens), 16)
                      if tokens[i:i + 16] != fixed[i:i + 16])
        print('    matrices changed: %d / %d' % (changed, count // 16))
        if apply:
            if changed == 0:
                print('    nothing to change - file already correct')
                continue
            backup = os.path.join(BACKUP_DIR, os.path.basename(path) + '.pre-bind-layout-fix')
            if not os.path.exists(backup):
                shutil.copy2(path, backup)
                print('    backup -> %s' % backup)
            new_text = text[:start] + open_tag + ' '.join(fixed) + close_tag + text[end:]
            open(path, 'w', encoding='utf-8').write(new_text)
            print('    WRITTEN: %s' % path)
    return 0


def main():
    ap = argparse.ArgumentParser(description='Fix COLLADA inverse bind matrix layout')
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--dry-run', action='store_true', help='validate and show changes, write nothing')
    g.add_argument('--apply', action='store_true', help='backup originals and write the fixed layout')
    args = ap.parse_args()
    print('APPLYING' if args.apply else 'DRY RUN - no writes')
    rc = 0
    for f in FILES:
        rc |= process(f, args.apply)
    sys.exit(rc)


if __name__ == '__main__':
    main()