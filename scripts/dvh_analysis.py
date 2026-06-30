#!/usr/bin/env python3
"""Extract DVH constraint results from all validation runs."""
import re, os, glob

files = sorted(glob.glob('resultados/*_s*.txt'))
print(f"{'File':<35} {'OK':>4} {'VIOL':>4} {'OK%':>6} {'D95_OK':>7} {'D2_OK':>7} {'Blad_OK':>7} {'Rect_OK':>7}")

for f in files:
    try:
        with open(f) as fh:
            text = fh.read()
        ok = len(re.findall(r'\bOK\b', text))
        viol = len(re.findall(r'\bVIOL\b', text))
        total = ok + viol
        pct = ok/total*100 if total else 0
        
        # Per-constraint
        d95 = 'OK' in re.findall(r'PTV_68.*D95.*(OK|VIOL)', text)[0] if re.findall(r'PTV_68.*D95.*(OK|VIOL)', text) else '?'
        d2 = 'OK' in re.findall(r'PTV_68.*D2.*(OK|VIOL)', text)[0] if re.findall(r'PTV_68.*D2.*(OK|VIOL)', text) else '?'
        blad = 'OK' in re.findall(r'Bladder.*Dmax.*(OK|VIOL)', text)[0] if re.findall(r'Bladder.*Dmax.*(OK|VIOL)', text) else '?'
        rect = 'OK' in re.findall(r'Rectum.*Dmax.*(OK|VIOL)', text)[0] if re.findall(r'Rectum.*Dmax.*(OK|VIOL)', text) else '?'
        
        base = os.path.basename(f)
        print(f"{base:<35} {ok:>4} {viol:>4} {pct:>5.0f}% {str(d95):>7} {str(d2):>7} {str(blad):>7} {str(rect):>7}")
    except Exception as e:
        print(f"{os.path.basename(f):<35} ERROR: {e}")
