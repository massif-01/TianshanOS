#define WS_REVIEWER_NO_MAIN
#include "test_reviewer.c"
/* Actual power callback, with only the power status input type replaced. */
typedef enum {TS_POWER_POLICY_STATE_NORMAL,TS_POWER_POLICY_STATE_LOW_VOLTAGE,TS_POWER_POLICY_STATE_SHUTDOWN,TS_POWER_POLICY_STATE_PROTECTED,TS_POWER_POLICY_STATE_RECOVERY}ts_power_policy_state_t;
enum {TS_POWER_POLICY_EVENT_STATE_CHANGED,TS_POWER_POLICY_EVENT_LOW_VOLTAGE,TS_POWER_POLICY_EVENT_COUNTDOWN_TICK,TS_POWER_POLICY_EVENT_SHUTDOWN_START,TS_POWER_POLICY_EVENT_PROTECTED,TS_POWER_POLICY_EVENT_RECOVERY_START,TS_POWER_POLICY_EVENT_RECOVERY_COMPLETE,TS_POWER_POLICY_EVENT_DEBUG_TICK};
typedef struct {ts_power_policy_state_t state;float current_voltage;unsigned countdown_remaining_sec,protection_count;}ts_power_policy_status_t;
#include <math.h>
#include <ctype.h>
static struct {bool collect_output,match_found,fail_matched,expect_matched,stop_on_match;char *output_buffer,*fail_pattern,*expect_pattern,*extract_pattern,*extracted_value;size_t output_capacity,output_len;char var_name[64];ts_ssh_config_t config;} *s_exec_params;
static bool s_exec_cancel_requested;
static void *s_exec_session;
typedef struct{char source_id[64],name[96];int flags;struct {int type;char str_val[256];int int_val;}value;}ts_auto_variable_t;
#define TS_AUTO_VAL_STRING 1
#define TS_AUTO_VAL_INT 2
static int ts_variable_upsert(ts_auto_variable_t*v){(void)v;return 0;}
static void ts_ssh_abort(void*s){(void)s;assert(!"network delivery must not abort remote commands");}
#define malloc tracked_malloc
#define heap_caps_malloc test_heap_malloc
#define free tracked_free
#include "power_caller.inc"
#undef free
#undef malloc
#undef heap_caps_malloc
static unsigned powers,errors;
#ifdef TS_WS_POWER_SLOTS
static unsigned outputs[32],terminals[32],logs,protection_order[64],protection_n;
static void observe_all(httpd_ws_frame_t*f){observe_topic(f);const char*p=(const char*)f->payload;if(strstr(p,"power_event")){powers++;if(protection_n<64){cJSON*j=cJSON_Parse(p);protection_order[protection_n++]=cJSON_GetObjectItem(j,"protection_count")->valueint;cJSON_Delete(j);}}if(strstr(p,"ssh_exec_output")||strstr(p,"ssh_output"))outputs[sent_fd]++;if(strstr(p,"ssh_exec_error")||strstr(p,"\"status\":\"error\""))errors++;if(strstr(p,"ssh_exec_done")||strstr(p,"ssh_exec_error"))terminals[sent_fd]++;if(!strcmp(p,"log"))logs++;}
static void exec_begin(void){s_exec_session_id++;s_exec_output_failed=false;s_exec_terminal_claimed=false;s_exec_terminal_ready=false;s_exec_result_active=true;assert(!s_exec_output_pending);assert(ts_ws_result_reserve(4096,&s_exec_terminal)==0);}
#endif
static void observe_power(httpd_ws_frame_t*f){if(strstr((char*)f->payload,"power_event"))powers++;if(strstr((char*)f->payload,"\"status\":\"error\""))errors++;}
static void power_change(void){ts_power_policy_status_t s={.state=TS_POWER_POLICY_STATE_PROTECTED,.current_voltage=10.0f,.protection_count=1};ts_event_t e={.id=TS_POWER_POLICY_EVENT_PROTECTED,.data=&s,.data_size=sizeof(s)};power_policy_event_handler(&e,NULL);}
int main(int argc,char**argv){
 assert(argc==2);cJSON_Hooks hooks={tracked_malloc,tracked_free};cJSON_InitHooks(&hooks);start();open_peer(1);sent_hook=observe_power;
 if(!strcmp(argv[1],"f1")){
  connect_request(false,false);queue_fail=true;ssh_send_output("chunk",5);queue_fail=false;test_now+=100000;progress();pump();
  bool handled=!s_ssh_running && errors>0;
  printf("F1 assertion: accepted SSH output enqueue failure reaches session and error frame: %s\n",handled?"PASS":"FAIL (stream still normal / no error frame)");
  s_ssh_running=false;poll_exit();current=(void*)1;finish();return handled?0:1;
 }
 if(!strcmp(argv[1],"f2")){
  ts_ws_peer_t p;assert(ts_ws_peer_get((void*)11,1,&p));ts_ws_peer_log_level(p,5);
  httpd_ws_frame_t f={.type=1,.payload=(uint8_t*)"log",.len=3};assert(ts_ws_transport_log(&f,1)==0);assert(ts_ws_transport_log(&f,1)==0);
  power_change();pump();bool handled=powers==1;
  printf("F2 assertion: protection transition survives ordinary log pool saturation: %s\n",handled?"PASS":"FAIL (no protection frame)");finish();return handled?0:1;
 }
#ifdef TS_WS_POWER_SLOTS
 sent_hook=observe_all;
 if(!strcmp(argv[1],"late")){
  connect_request(false,false);
  ts_ws_message_t*m=ts_ws_message_text("block",5);assert(ts_ws_transport_submit(s_ssh_peer,m,0,0,NULL,NULL)==0);ts_ws_message_release(m);
  ssh_send_output("out",3);assert(s_ssh_output_pending==1);ssh_send_status("closed","normal close");assert(s_ssh_terminal_ready && s_ssh_result_pending);
  sdk_one();queue_fail=true;worker_step();assert(s_ssh_output_failed && !s_ssh_output_pending);queue_fail=false;test_now+=100000;progress();pump();
  assert(errors==1 && !strstr(payloads[1],"normal close"));s_ssh_running=false;poll_exit();current=(void*)1;finish();
  puts("PASS late SSH rejection: prior output settles before terminal; closed becomes explicit incomplete-output error");return 0;
 }
 if(!strcmp(argv[1],"early")){
  connect_request(false,false);immediate=true;uint64_t settled=s_tx_stats.settled;
  ssh_send_output("ok",2);assert(!s_ssh_output_pending && s_tx_stats.settled==settled+1 && outputs[1]==1);
  send_fail=true;ssh_send_output("fail",4);send_fail=false;immediate=false;
  assert(s_tx_stats.settled==settled+2 && s_ssh_output_failed && !s_ssh_running && !s_ssh_output_pending);
  poll_exit();current=(void*)1;finish();puts("PASS callback-before-return and actual send failure: exactly one settlement, no reference leak");return 0;
 }
 if(!strcmp(argv[1],"identity")){
  connect_request(false,false);unsigned old=s_ssh_generation;
  ssh_send_output("old",3);close_peer(1);open_peer(1);
  connect_request(false,false);assert(s_ssh_generation==old); /* retained old producer blocks replacement */
  pump();assert(s_ssh_output_failed && outputs[1]==0);poll_exit();current=(void*)1;
  connect_request(false,false);assert(s_ssh_generation>old && s_ssh_running);
  ssh_output_done(old,1,ESP_FAIL);assert(s_ssh_running && !s_ssh_output_failed); /* delayed old notification is fenced */
  s_ssh_running=false;poll_exit();current=(void*)1;pump();finish();puts("PASS old connection/generation cannot receive old output or fail replacement session");return 0;
 }
 if(!strcmp(argv[1],"exec")){
  open_peer(2);exec_begin();unsigned sid=s_exec_session_id;uint64_t settled_before=s_tx_stats.settled;
  ssh_exec_output_callback("part",4,false,(void*)(uintptr_t)sid);assert(s_exec_output_pending==2);
  ssh_exec_terminal_publish("{\"type\":\"ssh_exec_done\"}",sid);assert(s_exec_terminal_ready);
  sdk_one();assert(outputs[1]==1 && !terminals[1]);queue_fail=true;worker_step();queue_fail=false;
  assert(s_exec_output_failed && !s_exec_output_pending);test_now+=100000;worker_step();pump();
  assert(outputs[1]==1 && outputs[2]==0 && terminals[1]==1 && terminals[2]==1 && errors==2);assert(s_tx_stats.settled==settled_before+4);
  assert(strstr(payloads[1],"not confirmed"));finish();
  puts("PASS actual exec callback: partial target failure is independent; no successful output replay and no false done");return 0;
 }
 if(!strcmp(argv[1],"continuous")){
  exec_begin();queue_fail=true;ssh_exec_output_callback("chunk",5,false,(void*)(uintptr_t)s_exec_session_id);queue_fail=false;
  test_now+=100000;progress();pump();assert(errors==1 && !s_exec_output_pending && !s_exec_result_active);
  unsigned frames_before=frames[1];ssh_exec_output_callback("later",5,false,(void*)(uintptr_t)s_exec_session_id);ssh_exec_terminal_publish("{\"type\":\"ssh_exec_done\"}",s_exec_session_id);pump();assert(frames[1]==frames_before && !s_exec_cancel_requested);
  finish();puts("PASS continuous exec: observation reports incomplete immediately; no remote abort/replay or later normal completion");return 0;
 }
 if(!strcmp(argv[1],"power_retry")){
  open_peer(2);queue_fail=true;power_change();assert(s_tx_stats.power_accepted==1 && !powers);
  worker_step();queue_fail=false;test_now+=100000;worker_step();sdk_one();worker_step();sdk_one();assert(powers==2);
  send_fail_fd=2;power_change();pump();send_fail_fd=-1;assert(powers==3 && s_tx_stats.power_failed==1);finish();
  puts("PASS protection queue recovery without new messages; partial socket failure never replays successful recipient");return 0;
 }
 if(!strcmp(argv[1],"capacity")){
  ts_ws_peer_t p;assert(ts_ws_peer_get((void*)11,1,&p));
  ts_ws_message_t*m=ts_ws_message_text("log",3);for(unsigned i=0;i<TS_WS_CONNECTIONS;i++)assert(ts_ws_transport_submit(p,m,0,0,NULL,NULL)==0);
  ts_ws_message_t*m2=ts_ws_message_text("two",3);assert(m2);
  unsigned before=allocations;fail_allocation=allocations+1;power_change();assert(allocations==before);fail_allocation=0;
  ts_ws_message_release(m);ts_ws_message_release(m2);pump();assert(powers==1);
  char*big=malloc(TS_WS_LEGACY_BYTES);memset(big,'x',TS_WS_LEGACY_BYTES);m=ts_ws_message_text(big,TS_WS_LEGACY_BYTES-1);m2=ts_ws_message_text(big,TS_WS_LEGACY_BYTES-TS_WS_POWER_BUDGET-1);free(big);assert(m&&m2);
  power_change();pump();assert(powers==2 && s_tx_stats.bytes<=TS_WS_TOTAL_BYTES);ts_ws_message_release(m);ts_ws_message_release(m2);
  ts_ws_reservation_t r[TS_WS_POWER_TRANSITIONS];for(unsigned i=0;i<TS_WS_POWER_TRANSITIONS;i++)assert(ts_ws_power_reserve(false,&r[i])==0);
  before=allocations;uint64_t rejected=s_tx_stats.power_rejected;power_change();assert(s_tx_stats.power_rejected==rejected+1 && allocations==before);
  ts_ws_reservation_t tick;assert(ts_ws_power_reserve(true,&tick)==0);ts_ws_reservation_release(&tick);
  for(unsigned i=0;i<TS_WS_POWER_TRANSITIONS;i++)ts_ws_reservation_release(&r[i]);
  ts_ws_peer_t peers[TS_WS_TOPIC_TX_SLOTS];for(unsigned i=0;i<TS_WS_TOPIC_TX_SLOTS;i++)peers[i]=p;
  assert(reserve_kind(peers,TS_WS_TOPIC_TX_SLOTS,MSG_POWER,TS_WS_POWER_BYTES,&r[0])==0);rejected=s_tx_stats.power_rejected;power_change();assert(s_tx_stats.power_rejected==rejected+1);ts_ws_reservation_release(&r[0]);
  power_change();pump();assert(powers==3);finish();puts("PASS separate ordinary slot/descriptor/byte exhaustion cannot consume power quota; power overload counted without construction");return 0;
 }
 if(!strcmp(argv[1],"retry_limit")){
  queue_fail=true;power_change();for(unsigned i=1;i<TS_WS_QUEUE_RETRIES;i++){test_now+=100000;worker_step();}
  assert(!ts_ws_transport_needs_flush() && s_tx_stats.power_failed==1 && s_tx_stats.power_settled==1 && s_tx_stats.rejected==TS_WS_QUEUE_RETRIES && !s_tx_stats.jobs && !s_tx_stats.bytes);queue_fail=false;finish();puts("PASS persistent SDK rejection has bounded retry and observable terminal settlement");return 0;
 }
 if(!strcmp(argv[1],"stop")){
  connect_request(false,false);s_exec_running=true;ssh_send_output("last",4);power_change();delay_hook=progress;
  assert(ts_webui_ws_stop((void*)11)==ESP_ERR_INVALID_STATE);delay_hook=NULL;assert(s_worker && s_state==DRAINING);
  for(unsigned i=0;i<10;i++)progress();assert(outputs[1]==1 && powers==1);s_exec_running=false;
  s_ssh_running=false;poll_exit();current=(void*)1;for(unsigned i=0;i<10;i++)progress();finish();puts("PASS busy stop retains output and protection drain; retry releases dependencies");return 0;
 }
 if(!strcmp(argv[1],"class")){
  connect_request(false,false);ssh_send_output("ordered",7);
  assert(s_ssh_output_pending==1);unsigned ordinary=0,topics_used=0;
  for(unsigned i=0;i<TS_WS_ALL_TX_SLOTS;i++)if(s_deliveries[i].state==TX_QUEUED||s_deliveries[i].state==TX_PREPARED){if(i<TS_WS_TOPIC_TX_SLOTS)topics_used++;if(i>=TS_WS_TOPIC_TX_SLOTS&&i<TS_WS_TX_SLOTS)ordinary++;}
  assert(ordinary==1 && topics_used==0);ts_ws_transport_cancel_topics();assert(s_ssh_output_pending==1);pump();assert(outputs[1]==1 && !s_ssh_output_pending);
  s_ssh_running=false;poll_exit();current=(void*)1;pump();finish();puts("PASS completion callbacks do not reclassify SSH output or subject it to telemetry cancellation");return 0;
 }
 if(!strcmp(argv[1],"power_order")){
  queue_fail=true;
  ts_power_policy_status_t status={.state=TS_POWER_POLICY_STATE_LOW_VOLTAGE,.current_voltage=11.5f};ts_event_t e={.id=TS_POWER_POLICY_EVENT_COUNTDOWN_TICK,.data=&status,.data_size=sizeof(status)};
  status.protection_count=99;power_policy_event_handler(&e,NULL);power_policy_event_handler(&e,NULL);assert(s_tx_stats.power_rejected==1);
  for(unsigned i=1;i<=4;i++){status.protection_count=i;e.id=TS_POWER_POLICY_EVENT_STATE_CHANGED;power_policy_event_handler(&e,NULL);}
  assert(s_tx_stats.power_accepted==5);status.protection_count=5;power_policy_event_handler(&e,NULL);assert(s_tx_stats.power_rejected==2);
  queue_fail=false;test_now+=100000;worker_step();pump();assert(protection_n==5 && protection_order[0]==99);for(unsigned i=1;i<=4;i++)assert(protection_order[i]==i);
  finish();puts("PASS ticks cannot evict transitions; four accepted transitions retain order, fifth overload is explicit");return 0;
 }
 if(!strcmp(argv[1],"timeout")){
  connect_request(false,false);ssh_send_output("last",4);power_change();
  assert(ts_webui_ws_stop((void*)11)==ESP_ERR_TIMEOUT);assert(s_worker && s_state==DRAINING);
  for(unsigned i=0;i<12;i++)progress();assert(outputs[1]==1 && powers==1);
  s_ssh_running=false;poll_exit();current=(void*)1;for(unsigned i=0;i<10;i++)progress();finish();puts("PASS timeout stop continues output/protection drain without new messages; retry completes");return 0;
 }
 if(!strcmp(argv[1],"fair")){
  ts_ws_peer_t p;assert(ts_ws_peer_get((void*)11,1,&p));ts_ws_peer_log_level(p,5);for(unsigned i=0;i<8;i++)subscribe(p,topics[i].name,1000);
  httpd_ws_frame_t f={.type=1,.payload=(uint8_t*)"log",.len=3};
  for(unsigned i=0;i<300;i++){
   test_now+=1000000;ts_ws_transport_log(&f,1);if(i%3==0)power_change();worker_step();sdk_one();assert(s_tx_stats.bytes<=TS_WS_TOTAL_BYTES && s_tx_stats.jobs<=TS_WS_ALL_TX_SLOTS);
  }
  pump();for(unsigned i=0;i<8;i++){printf("fair %s=%u\n",topics[i].name,observed_topics[i]);assert(observed_topics[i]>0);}assert(powers>0&&logs>0);finish();puts("PASS bounded sustained log/protection/topic competition: all eligible classes progress");return 0;
 }
#endif
 return 2;
}
