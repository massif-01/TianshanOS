#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
build=$(mktemp -d /tmp/ts-ws-reviewer.XXXXXX)
trap 'rm -rf "$build"' EXIT
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
cjson="${IDF_PATH:-/Users/massif/esp/v5.5.2/esp-idf}/components/json/cJSON"
python3 - "$build" <<'PY'
from pathlib import Path
import sys,re
s=Path('components/ts_webui/src/ts_webui_ws.c').read_text()
names=['ts_webui_log_stream_enable','has_log_clients','update_log_stream_state','set_client_role','ssh_terminal_done','ssh_generation_valid','ssh_status_valid','ssh_request_error','ssh_send_status','ssh_send_output','ssh_poll_task','ssh_cleanup','handle_ssh_connect','start_terminal_session','ts_webui_ws_stop','ssh_exec_terminal_publish','ws_handler']
parts=[]
for name in names:
 m=re.search(r'^(?:static )?(?:esp_err_t|bool|void) '+name+r'\([^;]*?\)\n\{',s,re.M);assert m,name
 parts.append(s[m.start():s.index('\n}',m.start())+2])
Path(sys.argv[1],'reviewer_ws.inc').write_text('\n'.join(parts))
for source,names,dest in [
 ('components/ts_net/src/ts_http_server.c',['http_handler_wrapper'],'reviewer_http.inc'),
 ('main/ts_core_init.c',['core_release_components','core_init_rollback','ts_core_start','ts_core_stop','ts_core_deinit'],'reviewer_core.inc'),
 ('components/ts_core/ts_service/src/ts_service.c',['stop_service_internal','ts_service_stop','ts_service_stop_all','ts_service_deinit'],'reviewer_service.inc')]:
 s=Path(source).read_text();parts=[]
 for name in names:
  m=re.search(r'^(?:static )?esp_err_t '+name+r'\([^;]*?\)\n\{',s,re.M);assert m,name
  parts.append(s[m.start():s.index('\n}',m.start())+2])
 Path(sys.argv[1],dest).write_text('\n'.join(parts))
PY
cc -std=gnu11 -g -Wno-deprecated-declarations -fsanitize=address,undefined -Itests/ws_subscriptions/stubs -Icomponents/ts_webui/include -Icomponents/ts_webui/src -I"$cjson" -I"$build" tests/ws_subscriptions/test_reviewer.c "$cjson/cJSON.c" -o "$build/reviewer"
"$build/reviewer"

cc -std=gnu11 -g -fsanitize=address,undefined -Itests/ws_subscriptions/stubs -I"$cjson" -I"$build" tests/ws_subscriptions/test_reviewer_service.c -o "$build/service"
"$build/service"
cc -std=gnu11 -g -fsanitize=address,undefined -Wno-deprecated-declarations -Icomponents/ts_core/ts_event/include -Itests/ws_subscriptions/stubs -Icomponents/ts_core/ts_event/src -I"$cjson" -I"$build" tests/ws_subscriptions/test_reviewer_core.c -o "$build/core"
"$build/core"

cc -std=gnu11 -g -fsanitize=address,undefined -Itests/ws_subscriptions/stubs -I"$cjson" -I"$build" tests/ws_subscriptions/test_reviewer_http.c -o "$build/http"
"$build/http"
