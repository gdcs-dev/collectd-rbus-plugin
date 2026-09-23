// Shared types for the collectd rbus plugin.
#ifndef RBUS_PLUGIN_TYPES_H
#define RBUS_PLUGIN_TYPES_H

#include <collectd/core/daemon/plugin.h>
#include <stdbool.h>

typedef enum {
  RBUS_PLUGIN_MODE_UNSET = 0,
  RBUS_PLUGIN_MODE_POLL,
  RBUS_PLUGIN_MODE_SUBSCRIBE
} rbus_plugin_mode_t;

#define RBUS_PLUGIN_NAME_LEN 256

// One configured `<Path "...">` block. `is_table` paths contain a single
// "{i}" placeholder split into table_prefix/table_suffix around it.
typedef struct rbus_path_config {
  char path[RBUS_PLUGIN_NAME_LEN];
  bool is_table;
  char table_prefix[RBUS_PLUGIN_NAME_LEN];
  char table_suffix[RBUS_PLUGIN_NAME_LEN];
  rbus_plugin_mode_t mode;
  double interval; // seconds; poll mode only
  char type[DATA_MAX_NAME_LEN];
  int value_type; // DS_TYPE_GAUGE / DS_TYPE_COUNTER / DS_TYPE_DERIVE / DS_TYPE_ABSOLUTE
  struct rbus_path_config *next;
} rbus_path_config_t;

void rbus_path_config_list_free(rbus_path_config_t *head);

// Minimal strlcpy-alike: copies at most dest_len-1 bytes from src into dest
// and always null-terminates. Used instead of collectd's sstrncpy() because
// that pulls in collectd/core/utils/common/common.h, which requires
// build-generated config macros this out-of-tree plugin doesn't have.
void rbus_strlcpy(char *dest, char const *src, size_t dest_len);

#endif
