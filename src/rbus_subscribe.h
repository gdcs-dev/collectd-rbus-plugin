// Background thread handling every Subscribe-mode <Path> block. Opens its
// own rbus handle (rbusEvent_Subscribe callbacks fire on this thread) and
// dispatches directly via plugin_dispatch_values(), which collectd documents
// as safe to call from any thread -- see design.md's "Plugin registration"
// decision for why this avoids a separate queue.
#ifndef RBUS_PLUGIN_SUBSCRIBE_H
#define RBUS_PLUGIN_SUBSCRIBE_H

#include "rbus_types.h"

typedef struct rbus_subscribe_ctx rbus_subscribe_ctx_t;

// Starts the subscribe thread for every Subscribe-mode entry in `configs`
// (the caller retains ownership of `configs` and must keep it alive until
// rbus_subscribe_stop returns). Returns NULL if no Subscribe-mode entry
// exists (nothing to do) or if the thread failed to start (logged).
rbus_subscribe_ctx_t *rbus_subscribe_start(rbus_path_config_t *configs);

// Signals the thread to unsubscribe everything, close its rbus handle, and
// stop; joins it before returning.
void rbus_subscribe_stop(rbus_subscribe_ctx_t *ctx);

#endif
