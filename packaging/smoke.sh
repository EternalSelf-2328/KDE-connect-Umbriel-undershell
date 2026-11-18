#!/usr/bin/env bash
# Exercise installed files in a private bus/config without a real phone or display.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${1:-/usr}"
OUT="${2:?usage: smoke.sh PREFIX OUTPUT_DIRECTORY}"
mkdir -p "$OUT"
[[ -x "$PREFIX/bin/umbriel-connect" && -x "$PREFIX/bin/umbriel-connect-remote" ]]
"$PREFIX/bin/umbriel-connect-engine" --check
python3 - "$PREFIX" <<'PY'
from pathlib import Path
import subprocess,sys
prefix=Path(sys.argv[1]);count=0
candidates=list((prefix/'bin').glob('umbriel-connect*'))+list((prefix/'bin').glob('.umbriel-connect*-wrapped'))+list((prefix/'lib/umbriel-connect/engine').rglob('*'))
for p in candidates:
  if not p.is_file() or p.is_symlink():continue
  with p.open('rb') as f:magic=f.read(4)
  if magic!=b'\x7fELF':continue
  result=subprocess.run(['ldd',str(p)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  assert 'not found' not in result.stdout,(str(p),result.stdout);count+=1
print('Installed ELF dependencies resolved:',count)
PY
bash "$ROOT/tests/private-session.sh" "$PREFIX/bin/umbriel-connect" --preview "$OUT/home.png"
for tool in features messages; do
 bash "$ROOT/tests/private-session.sh" env UMBRIEL_CONNECT_PREVIEW_OUTPUT="$OUT/$tool.png" timeout 25 "$PREFIX/bin/umbriel-connect-$tool"
done
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys
for name in ['home','features','messages']:
 p=Path(sys.argv[1])/(name+'.png');assert p.stat().st_size>1000 and p.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'),name
print('Installed desktop, private features and messages rendered successfully')
PY
