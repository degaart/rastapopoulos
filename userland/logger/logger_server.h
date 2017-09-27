#pragma once

/* Handlers */
void handle_logger_trace(int sender_pid, const char* msg);

/* dispatcher */
void rpc_dispatch(int);

