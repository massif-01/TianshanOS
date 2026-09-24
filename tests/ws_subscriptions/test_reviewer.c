/* Compose actual manager/transport with extracted, unmodified WebUI functions. */
#define main existing_manager_main
#include "test_manager.c"
#undef main
#include <setjmp.h>
#include <stdatomic.h>
#define MAX_WS_CLIENTS TS_WS_CONNECTIONS
#define TS_LOG_NONE 0
#define TS_LOG_ERROR 1
#define TS_LOG_VERBOSE 5
#define HTTP_GET 0
#define HTTPD_WS_TYPE_CLOSE 8
#define CONFIG_TS_LOG_BUFFER_SIZE 16
#define SSH_OUTPUT_BUF_SIZE 4096
#define TS_SSH_AUTH_PASSWORD 1
typedef int ts_log_level_t;
typedef enum {WS_CLIENT_TYPE_EVENT,WS_CLIENT_TYPE_TERMINAL,WS_CLIENT_TYPE_SSH_SHELL,WS_CLIENT_TYPE_LOG} ws_client_type_t;
static struct {bool active;int fd;httpd_handle_t hd;ws_client_type_t type;int log_min_level;}s_clients[MAX_WS_CLIENTS];
static httpd_handle_t s_server=(void*)11;
static bool s_ws_stopping,s_exec_running;
static atomic_bool s_exec_terminal_claimed;
static atomic_uint s_exec_creators;
static ts_event_handler_handle_t s_power_event_handle;
static ts_ws_peer_t s_ssh_peer;
static int s_ssh_client_fd=-1,s_terminal_client_fd=-1;
static void *s_ssh_session,*s_ssh_shell;
static atomic_bool s_ssh_running,s_ssh_poll_alive;
enum {SSH_IDLE,SSH_STARTING,SSH_READY,SSH_TERMINAL};
static atomic_uint s_ssh_state,s_ssh_generation,s_ssh_result_pending;
static ts_ws_reservation_t s_ssh_terminal,s_exec_terminal;
static atomic_uint s_ssh_output_pending,s_exec_output_pending;
static atomic_bool s_ssh_output_failed,s_exec_output_failed,s_ssh_terminal_ready,s_exec_terminal_ready,s_exec_result_active;
static unsigned s_exec_session_id;
static ts_ws_peer_t s_exec_result_peers[TS_WS_CONNECTIONS];
static unsigned s_exec_result_count;
static void ssh_terminal_flush(void);
static void ssh_exec_terminal_flush(void);
static void ssh_exec_terminal_publish(const char*,uint32_t);
static bool shell_active,create_fails,close_on_create;
static void update_log_stream_state(void);
static bool has_log_clients(void);
static int ts_webui_log_stream_enable(bool enabled);
static void*s_log_callback_handle;
static atomic_bool s_log_streaming_enabled;
static int remove_log_result;
static const char *incoming;
static unsigned commands;
static const char*s_level_names[]={"NONE","ERROR","WARN","INFO","DEBUG","VERBOSE"};
typedef struct{int timestamp_ms,level;const char*tag,*message,*task_name;}ts_log_entry_t;
static void log_ws_callback(const ts_log_entry_t*e,void*u){(void)e;(void)u;}
static int ts_log_add_callback(void(*fn)(const ts_log_entry_t*,void*),int level,void*u,void**h){(void)fn;(void)level;(void)u;*h=(void*)1;return 0;}
static int ts_log_remove_callback(void*h){(void)h;return remove_log_result;}
static size_t ts_log_buffer_search(ts_log_entry_t*e,size_t n,int a,int b,void*t,void*m){(void)e;(void)n;(void)a;(void)b;(void)t;(void)m;return 0;}
static void*test_heap_malloc(size_t n,unsigned caps){(void)caps;return tracked_malloc(n);}
static int add_client(httpd_req_t*r,ws_client_type_t t){(void)r;(void)t;commands++;return 0;}
static int httpd_ws_recv_frame(httpd_req_t*r,httpd_ws_frame_t*f,size_t n){(void)r;f->type=HTTPD_WS_TYPE_TEXT;f->len=strlen(incoming);if(n)memcpy(f->payload,incoming,f->len);return 0;}
static void handle_terminal_command(httpd_req_t*r,const char*d){(void)r;(void)d;commands++;}
static void ts_console_request_interrupt(void){commands++;}
static void handle_ssh_input(const char*d){(void)d;commands++;}
static void handle_ssh_disconnect(void){commands++;}
static void handle_ssh_signal(const char*s){(void)s;commands++;}
static void handle_ssh_resize(int w,int h){(void)w;(void)h;commands++;}
static int httpd_ws_send_frame(httpd_req_t*r,httpd_ws_frame_t*f){return httpd_ws_send_frame_async(r->handle,r->fd,f);}
static void ts_console_clear_output_cb(void){}
static void terminal_output_cb(const char*d,size_t n,void*u){(void)d;(void)n;(void)u;}
static void ts_console_set_output_cb(void(*fn)(const char*,size_t,void*),void*u){(void)fn;(void)u;}
static void cleanup_disconnected_client(int fd){(void)fd;}
static void ssh_cleanup(void);
static esp_err_t ssh_send_status(const char*,const char*);
static void ssh_send_output(const char*d,size_t n);
typedef struct {const char*host,*username;int port,auth_method,timeout_ms;struct {const char*password;}auth;}ts_ssh_config_t;
typedef struct {int term_width,term_height,read_timeout_ms;}ts_shell_config_t;
#define TS_SSH_DEFAULT_CONFIG() ((ts_ssh_config_t){0})
#define TS_SHELL_DEFAULT_CONFIG() ((ts_shell_config_t){0})
static int ts_ssh_session_create(const ts_ssh_config_t*c,void**out){(void)c;*out=(void*)5;return 0;}
static int ts_ssh_connect(void*s){(void)s;return 0;}
static const char*ts_ssh_get_error(void*s){(void)s;return "fixture";}
static int ts_ssh_shell_open(void*s,const ts_shell_config_t*c,void**out){(void)s;(void)c;*out=(void*)6;shell_active=true;return 0;}
static bool ts_ssh_shell_is_active(void*s){return s && shell_active;}
static int ts_ssh_shell_read(void*s,char*b,size_t n,size_t*out){(void)s;(void)b;(void)n;*out=0;return 0;}
static void ts_ssh_shell_close(void*s){(void)s;shell_active=false;}
static void ts_ssh_disconnect(void*s){(void)s;}
static void ts_ssh_session_destroy(void*s){(void)s;}
static int xTaskCreateWithCaps(void(*fn)(void*),const char*n,unsigned size,void*a,unsigned prio,void*out,unsigned caps){
 (void)fn;(void)n;(void)size;(void)a;(void)prio;(void)out;(void)caps;
 if(close_on_create)shell_active=false;
 return create_fails?0:pdPASS;
}
#define free tracked_free
#define malloc tracked_malloc
#define heap_caps_malloc test_heap_malloc
#include "reviewer_ws.inc"
#undef free
#undef malloc
#undef heap_caps_malloc
static jmp_buf worker_yield;
static void yield_worker(void){longjmp(worker_yield,1);}
static void worker_step(void){
 wait_hook=yield_worker;current=(void*)2;
 if(!setjmp(worker_yield))worker_fn(NULL);
 current=(void*)1;wait_hook=NULL;
}
static void sdk_one(void){
 if(!queued)return;
 current=(void*)3;typeof(queue[0]) q=queue[0];memmove(queue,queue+1,--queued*sizeof(queue[0]));q.fn(q.arg);current=(void*)1;
}
static void progress(void){sdk_one();if(s_worker){if(s_state==STOPPING)worker_exit();else worker_step();}}
static void poll_exit(void){current=(void*)4;ssh_poll_task(NULL);current=(void*)3;}
static void finish(void){
 s_exec_running=false;delay_hook=progress;assert(ts_webui_ws_stop((void*)11)==ESP_OK);delay_hook=NULL;
 for(int i=0;i<32;i++)if(reqs[i].sess_ctx)close_peer(i);
 ts_ws_transport_stopped((void*)11);assert(!live_allocations);
 s_ws_stopping=false;
}
static void connect_request(bool fail,bool close_early){
 create_fails=fail;close_on_create=close_early;
 cJSON*params=cJSON_Parse("{\"host\":\"fixture\",\"user\":\"fixture\"}");
 current=(void*)3;delay_hook=poll_exit;handle_ssh_connect(&reqs[1],params);delay_hook=NULL;current=(void*)1;cJSON_Delete(params);
}
static unsigned observed_topics[8];
static void observe_topic(httpd_ws_frame_t*f){
 for(unsigned i=0;i<8;i++){
  char match[80];snprintf(match,sizeof(match),"\"topic\":\"%s\"",topics[i].name);
  if(strstr((const char*)f->payload,match))observed_topics[i]++;
 }
}

#ifndef WS_REVIEWER_NO_MAIN
int main(void){
 cJSON_Hooks hooks={tracked_malloc,tracked_free};cJSON_InitHooks(&hooks);
 start();open_peer(1);open_peer(2);s_exec_terminal_claimed=false;assert(ts_ws_result_reserve(4096,&s_exec_terminal)==0);s_exec_running=true;delay_hook=progress;
 assert(ts_webui_ws_stop((void*)11)==ESP_ERR_INVALID_STATE);delay_hook=NULL;
 assert(s_worker && s_state==DRAINING);
 reqs[1].method=1;incoming="{\"type\":\"ping\"}";current=(void*)3;
 assert(ws_handler(&reqs[1])==0 && strstr(payloads[1],"pong"));
 incoming="{\"type\":\"terminal_input\",\"data\":\"new-command\"}";
 assert(ws_handler(&reqs[1])==0 && !commands && strstr(payloads[1],"stopping"));current=(void*)1;
 unsigned a=frames[1],b=frames[2];
 httpd_ws_frame_t result={.payload=(uint8_t*)"final-result",.len=12};
 ssh_exec_terminal_publish("{\"type\":\"ssh_exec_done\",\"session_id\":1}",1);
 sdk_one();assert(!queued && ts_ws_transport_needs_flush());
 worker_step();sdk_one();assert(frames[1]==a+1 && frames[2]==b+1);
 assert(!transport_server.outstanding);finish();
 puts("PASS R1: busy stop retains real worker; final fanout drains with no new producer message");
 start();open_peer(1);open_peer(2);
 assert(ts_webui_ws_stop((void*)11)==ESP_ERR_TIMEOUT && s_worker && s_state==DRAINING);
 a=frames[1];b=frames[2];assert(ts_ws_transport_broadcast(&result)==0);
 for(unsigned i=0;i<4;i++){sdk_one();worker_step();}
 assert(frames[1]==a+1 && frames[2]==b+1);finish();
 start();open_peer(1);open_peer(2);a=frames[1];b=frames[2];
 assert(ts_ws_transport_broadcast(&result)==0);assert(ts_ws_subscriptions_pause()==0);
 assert(ts_ws_transport_stop((void*)11,1)==ESP_ERR_TIMEOUT && s_worker);
 sdk_one();worker_step();sdk_one();assert(frames[1]==a+1 && frames[2]==b+1);finish();
 puts("PASS R1 timeout: handler-barrier and transport-drain failures retain worker; accepted fanout progresses before retry");


 start();ts_ws_peer_t peer=open_peer(1);
 s_clients[0].active=true;s_clients[0].fd=1;s_clients[0].hd=peer.server;
 set_client_role(peer,WS_CLIENT_TYPE_LOG,5);
 httpd_ws_frame_t log={.payload=(uint8_t*)"old-log",.len=7};
 a=frames[1];assert(ts_ws_transport_log(&log,3)==0);
 current=(void*)3;start_terminal_session(&reqs[1]);current=(void*)1;
 assert(s_clients[0].type==WS_CLIENT_TYPE_TERMINAL);pump();assert(frames[1]==a+1); /* welcome only */
 set_client_role(peer,WS_CLIENT_TYPE_LOG,5);
 assert(ts_ws_transport_log(&log,3)==0);set_client_role(peer,WS_CLIENT_TYPE_EVENT,0);set_client_role(peer,WS_CLIENT_TYPE_LOG,5);
 a=frames[1];pump();assert(frames[1]==a); /* old generation cannot revive */
 assert(ts_ws_transport_log(&log,3)==0);pump();assert(frames[1]==a+1);
 finish();s_terminal_client_fd=-1;
 assert(ts_webui_log_stream_enable(true)==0);remove_log_result=ESP_ERR_INVALID_STATE;
 assert(ts_webui_log_stream_enable(false)==ESP_ERR_INVALID_STATE && s_log_callback_handle && !s_log_streaming_enabled);
 assert(ts_webui_log_stream_enable(true)==0 && s_log_streaming_enabled);remove_log_result=0;assert(ts_webui_log_stream_enable(false)==0);
 puts("PASS R3: real role change drops old queued logs, including unsubscribe/resubscribe ABA");

 start();peer=open_peer(1);s_clients[0].active=true;s_clients[0].fd=1;s_clients[0].hd=peer.server;
 /* Occupy both ordinary buffers; a rejected startup still has its result slot. */
 ts_ws_message_t*m1=ts_ws_message_text("one",3),*m2=ts_ws_message_text("two",3);assert(m1&&m2);
 set_client_role(peer,WS_CLIENT_TYPE_LOG,5);
 connect_request(true,false);assert(s_clients[0].type!=WS_CLIENT_TYPE_LOG);assert(s_ssh_state==SSH_TERMINAL && !s_ssh_poll_alive && !s_ssh_session);
 unsigned generation=s_ssh_generation;
 connect_request(false,false);assert(s_ssh_generation==generation && strstr(payloads[1],"still pending"));
 ts_ws_message_release(m1);ts_ws_message_release(m2);pump();assert(strstr(payloads[1],"Failed to create SSH session"));
 connect_request(false,true);pump();assert(strstr(payloads[1],"closed during startup"));assert(!s_ssh_session);
 connect_request(false,false);assert(s_ssh_state==SSH_READY && strstr(payloads[1],"connected"));
 /* Queue a READY from a background caller, then close before it executes. */
 assert(ssh_send_status("connected","late readiness")==0);
 unsigned old_generation=s_ssh_generation;
 queue_fail=true;assert(ssh_send_status("error","fixture failure")==0);
 sdk_one();worker_step();assert(!queued && ts_ws_transport_needs_flush());queue_fail=false;
 assert(!ssh_status_valid(old_generation,SSH_READY));
 test_now+=100000;worker_step();pump();assert(strstr(payloads[1],"fixture failure"));
 current=(void*)3;delay_hook=poll_exit;ssh_cleanup();delay_hook=NULL;current=(void*)1;
 connect_request(false,false);
 ts_ws_message_t *full1=ts_ws_message_text("full1",5),*full2=ts_ws_message_text("full2",5);assert(full1&&full2);
 ssh_send_output("payload",7);assert(s_ssh_state==SSH_TERMINAL && !s_ssh_running);
 poll_exit();current=(void*)1;ts_ws_message_release(full1);ts_ws_message_release(full2);pump();
 assert(strstr(payloads[1],"SSH output delivery incomplete") && !s_ssh_session && !s_ssh_poll_alive);
 finish();
 puts("PASS R2: task-create failure/early close never end as connected; reserved failure survives normal pool saturation and SDK queue rejection; late READY invalid");

 start();open_peer(1);s_exec_terminal_claimed=false;assert(ts_ws_result_reserve(TS_WS_LEGACY_BYTES,&s_exec_terminal)==0);
 m1=ts_ws_message_text("one",3);m2=ts_ws_message_text("two",3);assert(m1&&m2);
 open_peer(2);a=frames[1];b=frames[2];
 ssh_exec_terminal_publish("{\"type\":\"ssh_exec_done\",\"session_id\":7}",7);
 ts_ws_message_release(m1);ts_ws_message_release(m2);pump();assert(frames[1]==a+1&&frames[2]==b+1);finish();
 puts("PASS R2 exec: admitted result reservation survives ordinary saturation and preserves recipients at completion");

 start();peer=open_peer(1);uint64_t serial[8];for(unsigned i=0;i<8;i++){serial[i]=s_stats.serialized[i];subscribe(peer,topics[i].name,10000);}
 sent_hook=observe_topic;
 for(unsigned cycle=0;cycle<100;cycle++) {test_now+=10000000;run_batch();pump();}
 for(unsigned i=0;i<8;i++) {
  assert(observed_topics[i]>50 && observed_topics[i]==s_stats.serialized[i]-serial[i]);
  printf("R5 observed %s: %u delivered frames\n",topics[i].name,observed_topics[i]);
 }
 /* Freeing capacity services still-due targets without waiting their next interval. */
 for(unsigned i=0;i<8;i++)serial[i]=s_stats.serialized[i];
 unsigned improved=0;run_batch();pump();for(unsigned i=0;i<8;i++)improved+=s_stats.serialized[i]>serial[i];assert(improved>=2);
 ts_ws_message_t*held[6];cJSON*j=cJSON_CreateObject();for(unsigned i=0;i<6;i++){held[i]=ts_ws_message_json("held",j,0);assert(held[i]);}
 test_now+=10000000;unsigned before=total_reads();run_batch();assert(total_reads()==before);
 for(unsigned i=0;i<6;i++)ts_ws_message_release(held[i]);cJSON_Delete(j);
 run_batch();pump();assert(total_reads()>before);
 ts_ws_peer_t pinned[TS_WS_TOPIC_TX_SLOTS];for(unsigned i=0;i<TS_WS_TOPIC_TX_SLOTS;i++)pinned[i]=peer;
 ts_ws_reservation_t reserved;assert(ts_ws_reserve(pinned,TS_WS_TOPIC_TX_SLOTS,false,TS_WS_FRAME_BYTES,&reserved)==0);
 test_now+=10000000;before=total_reads();run_batch();assert(total_reads()==before);
 ts_ws_reservation_release(&reserved);run_batch();pump();assert(total_reads()>before);
 char*large=malloc(TS_WS_LEGACY_BYTES);memset(large,'x',TS_WS_LEGACY_BYTES);
 m1=ts_ws_message_text(large,TS_WS_LEGACY_BYTES-1);m2=ts_ws_message_text(large,TS_WS_LEGACY_BYTES-TS_WS_POWER_BUDGET-1);assert(m1&&m2);free(large);
 test_now+=10000000;before=total_reads();run_batch();assert(total_reads()==before);
 ts_ws_message_release(m1);ts_ws_message_release(m2);run_batch();pump();assert(total_reads()>before);finish();
 puts("PASS R5: all eight topics progress under six-slot pressure; no API collection without message/descriptor/byte capacity");
}

#endif
