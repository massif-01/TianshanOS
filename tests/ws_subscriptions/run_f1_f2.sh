#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
build=$(mktemp -d /tmp/ts-ws-f1-f2.XXXXXX)
trap 'rm -rf "$build"' EXIT
if [[ "${1:-}" == "--baseline" ]]; then
  tar -xzf docs/repair/ws-f1-f2/pre-fix-source.tar.gz -C "$build"
  python3 - "$build" <<'HASH'
import json,hashlib,sys
from pathlib import Path
for name,want in json.loads(Path('docs/repair/ws-f1-f2/baseline.json').read_text())['sha256'].items():
 assert hashlib.sha256((Path(sys.argv[1])/name).read_bytes()).hexdigest()==want,name
print('Frozen production snapshot hashes verified')
HASH
  # Current assertions/harness against unchanged frozen production sources.
  cp tests/ws_subscriptions/{test_f1_f2.c,test_reviewer.c,run_f1_f2.sh} "$build/tests/ws_subscriptions/"
  cp tests/ws_subscriptions/stubs/platform.h "$build/tests/ws_subscriptions/stubs/"
  if bash "$build/tests/ws_subscriptions/run_f1_f2.sh" > "$build/red.txt" 2>&1; then
    cat "$build/red.txt"; echo 'Unexpected baseline PASS'; exit 1
  fi
  cat "$build/red.txt"
  grep -q 'F1 assertion:.*FAIL' "$build/red.txt"
  grep -q 'F2 assertion:.*FAIL' "$build/red.txt"
  if grep -Eq 'error:|Assertion failed|Sanitizer' "$build/red.txt"; then exit 1; fi
  echo 'CONFIRMED: both failures are behavioral assertions, not compilation or cleanup failures'
  exit 0
fi
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
cjson="${IDF_PATH:-/Users/massif/esp/v5.5.2/esp-idf}/components/json/cJSON"
python3 - "$build" <<'PY'
from pathlib import Path
import re,sys
script=Path('tests/ws_subscriptions/run_reviewer.sh').read_text()
code=script.split("<<'PY'\n",1)[1].split('\nPY',1)[0]
exec(compile(code,'extract-reviewer','exec'))
s=Path('components/ts_webui/src/ts_webui_ws.c').read_text();parts=[]
for name in ['power_state_to_string','power_policy_event_handler','ts_webui_broadcast','simple_pattern_match','ssh_exec_output_callback']:
 m=re.search(r'^(?:static )?(?:const char \*|void |esp_err_t |bool )'+name+r'\([^;]*?\)\n\{',s,re.M);assert m,name
 parts.append(s[m.start():s.index('\n}',m.start())+2])
Path(sys.argv[1],'power_caller.inc').write_text('\n'.join(parts))
PY
"${CC:-cc}" -std=gnu11 -g -Wno-deprecated-declarations -fsanitize=address,undefined -Itests/ws_subscriptions/stubs -Icomponents/ts_webui/include -Icomponents/ts_webui/src -I"$cjson" -I"$build" tests/ws_subscriptions/test_f1_f2.c "$cjson/cJSON.c" -o "$build/f1f2"
failed=0
scenarios="f1 f2"
if grep -q TS_WS_POWER_SLOTS components/ts_webui/include/ts_ws_transport.h; then scenarios="$scenarios late early identity exec continuous power_retry capacity retry_limit stop timeout class power_order fair"; fi
for scenario in $scenarios; do
 if ! "$build/f1f2" "$scenario"; then failed=1; fi
done
exit "$failed"
