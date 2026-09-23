// Parses <Plugin rbus><Path "..."> ... </Path></Plugin> config blocks into
// rbus_path_config_t entries. Kept independent of collectd's daemon runtime:
// it only reads the oconfig_item_t tree collectd (or a test) hands it.
#ifndef RBUS_PLUGIN_CONFIG_H
#define RBUS_PLUGIN_CONFIG_H

#include <collectd/liboconfig/oconfig.h>
#include <stddef.h>

#include "rbus_types.h"

// Parses a single <Path "..."> block. On success fills *out and returns true.
// On failure writes a human-readable reason to errbuf and returns false;
// the caller must reject (skip) that block without aborting the rest of
// config load.
bool rbus_config_parse_path_block(oconfig_item_t *path_block,
                                   rbus_path_config_t *out, char *errbuf,
                                   size_t errbuf_len);

// Parses every <Path> child of the <Plugin rbus> block `ci`. Valid blocks are
// prepended to *out_head (caller owns the resulting list; free with
// rbus_path_config_list_free). Invalid blocks are logged and skipped, not
// fatal. Returns the number of valid blocks parsed.
int rbus_config_parse_plugin(oconfig_item_t *ci, rbus_path_config_t **out_head);

#endif
