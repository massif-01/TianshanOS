#pragma once
/* Internal lifecycle; all returned operation pointers carry a reference. */
#include "ts_ws_transport.h"
typedef enum { TS_WS_OP_SHELL, TS_WS_OP_EXEC, TS_WS_OP_KINDS } ts_ws_op_kind_t;
typedef enum { OP_FREE, OP_STARTING, OP_OPEN, OP_CLOSING, OP_FINALIZING,
               OP_TERMINAL_PENDING, OP_TERMINAL_SETTLED, OP_RECLAIMING } ts_ws_op_phase_t;
typedef struct ts_ws_operation ts_ws_operation_t;
typedef struct {
    ts_ws_operation_t *op;
    uint64_t sequence; /* single producer order within this operation */
    ts_ws_peer_t peers[TS_WS_CONNECTIONS];
    unsigned count;
} ts_ws_output_ticket_t;
typedef struct {
    ts_ws_op_phase_t phase;
    uint64_t identity, tickets, finished, registered, settled, finalizers, duplicates;
    unsigned refs, executors, preparing, unsettled, terminals;
    bool failed;
} ts_ws_op_stats_t;
void ts_ws_op_enable(void);
void ts_ws_op_stop_admission(void);
bool ts_ws_op_busy(void);
ts_ws_operation_t *ts_ws_op_create(ts_ws_op_kind_t kind, ts_ws_peer_t peer, esp_err_t *error);
void ts_ws_op_bind(ts_ws_operation_t *op, void *data, void (*destroy)(void *));
void *ts_ws_op_data(ts_ws_operation_t *op); /* caller owns a reference */
uint32_t ts_ws_op_id(ts_ws_operation_t *op);
ts_ws_operation_t *ts_ws_op_acquire(ts_ws_op_kind_t kind, uint32_t id);
void ts_ws_op_retain(ts_ws_operation_t *op);
void ts_ws_op_release(ts_ws_operation_t *op);
void ts_ws_op_executor_take(ts_ws_operation_t *op);
void ts_ws_op_executor_done(ts_ws_operation_t *op);
bool ts_ws_op_executing(ts_ws_operation_t *op);
bool ts_ws_op_open(ts_ws_operation_t *op);
bool ts_ws_op_is_open(ts_ws_operation_t *op);
bool ts_ws_op_starting(ts_ws_operation_t *op);
ts_ws_peer_t ts_ws_op_peer(ts_ws_operation_t *op);
void ts_ws_op_discard(ts_ws_operation_t *op); /* unaccepted creation */
bool ts_ws_op_output_begin(ts_ws_operation_t *op, ts_ws_output_ticket_t *ticket);
void ts_ws_op_output_finish(ts_ws_output_ticket_t *ticket, const char *json);
/* winner owns a close builder until close_finish, before any encoding */
bool ts_ws_op_close_begin(ts_ws_operation_t *op);
void ts_ws_op_close_finish(ts_ws_operation_t *op, const char *json);
void ts_ws_op_poll(void); /* existing subscription worker, no new task */
void ts_ws_op_stats(ts_ws_op_kind_t kind, ts_ws_op_stats_t *stats);

void ts_ws_op_set_maintenance(ts_ws_operation_t *op,bool (*maintain)(void *));
bool ts_ws_op_needs_tick(void);
