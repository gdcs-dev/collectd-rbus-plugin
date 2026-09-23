// Discovers and collects TR-181 table-row values via rbus's own wildcard
// query (Option 4 of rbus_getExt: replacing the "{i}" placeholder with "*"
// returns one property per currently-existing row in a single bus call).
// Used both by the polling path (called every interval) and the subscribe
// path (called again whenever a row-added/row-removed event fires), per
// design.md's "Table row discovery" decision.
#ifndef RBUS_PLUGIN_TABLES_H
#define RBUS_PLUGIN_TABLES_H

#include <rbus.h>

#include "rbus_types.h"

// Re-discovers every current row for `cfg` (a table path) and dispatches one
// value per row. Returns the number of rows successfully dispatched, or -1
// if the rbus_getExt call itself failed.
//
// When `out_instances` is non-NULL, it is filled (up to `max_instances`
// entries) with the decimal row-instance strings discovered, and
// *out_count is set to how many were written -- used by the subscribe path
// to know which per-row VALUE_CHANGED subscriptions to add/remove.
int rbus_tables_collect(rbusHandle_t handle, rbus_path_config_t const *cfg,
                         double interval,
                         char out_instances[][RBUS_PLUGIN_NAME_LEN],
                         int max_instances, int *out_count);

#endif
