// Builds a collectd value_list_t from a mapped rbus value and dispatches it
// via plugin_dispatch_values(). Shared by both the polling and subscribe
// acquisition paths so they differ only in what triggers a read (per
// design.md).
#ifndef RBUS_PLUGIN_DISPATCH_H
#define RBUS_PLUGIN_DISPATCH_H

#include <rbus.h>

#include "rbus_types.h"

// Caches collectd's configured default Interval (seconds) for use by
// dispatch calls made from threads collectd doesn't manage (the subscribe
// thread) -- plugin_get_interval() only works from a thread collectd itself
// set a plugin context for, and warns ("UNKNOWN plugin") otherwise. Call
// once from the init callback, which does run on such a thread.
void rbus_dispatch_set_default_interval(double seconds);

// Dispatches one value for `cfg`. `plugin_instance` is the row index as a
// decimal string for table paths, or NULL for scalar paths. `interval`, if
// nonzero, overrides collectd's default interval (used for Poll-mode paths
// so their reported interval matches their configured one).
//
// Returns false without dispatching when `rv`'s rbus type cannot be
// represented as cfg->value_type (per the "Typed value mapping"
// requirement); the caller is expected to log/count this as a collection
// error, not treat it as fatal.
bool rbus_dispatch_value(rbus_path_config_t const *cfg,
                          char const *plugin_instance, double interval,
                          rbusValue_t rv);

#endif
