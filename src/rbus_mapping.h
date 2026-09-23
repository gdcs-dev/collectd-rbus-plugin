// Maps an rbus value to a collectd value_t of a declared DS_TYPE_*. Pure
// translation logic with no collectd-daemon or rbus-bus dependency beyond the
// rbusValue_t accessor calls, so it is unit-testable on its own.
#ifndef RBUS_PLUGIN_MAPPING_H
#define RBUS_PLUGIN_MAPPING_H

#include <collectd/core/daemon/plugin.h>
#include <rbus.h>
#include <stdbool.h>

// Converts `rv` into *out per the declared collectd value_type
// (DS_TYPE_GAUGE/COUNTER/DERIVE/ABSOLUTE). Returns false without touching
// *out if `rv`'s rbus type cannot be represented as value_type -- e.g. a
// string or a negative number mapped to COUNTER/ABSOLUTE.
bool rbus_mapping_convert(rbusValue_t rv, int value_type, value_t *out);

#endif
