#include "rbus_tables.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rbus_dispatch.h"

// Recovers the row index string from a discovered property name, e.g.
// name="Device.Bridging.Bridge.1.Port.5.Stats.BytesSent",
// prefix="Device.Bridging.Bridge.1.Port.", suffix=".Stats.BytesSent"
// yields "5". Rejects a non-numeric or empty instance rather than guessing.
static bool extract_instance(char const *name, char const *prefix,
                              char const *suffix, char *out, size_t out_len) {
  size_t prefix_len = strlen(prefix);
  size_t suffix_len = strlen(suffix);
  size_t name_len = strlen(name);

  if (name_len <= prefix_len + suffix_len)
    return false;
  if (strncmp(name, prefix, prefix_len) != 0)
    return false;
  if (strcmp(name + name_len - suffix_len, suffix) != 0)
    return false;

  size_t inst_len = name_len - prefix_len - suffix_len;
  if (inst_len == 0 || inst_len >= out_len)
    return false;

  memcpy(out, name + prefix_len, inst_len);
  out[inst_len] = '\0';
  for (size_t i = 0; i < inst_len; i++) {
    if (!isdigit((unsigned char)out[i]))
      return false;
  }
  return true;
}

int rbus_tables_collect(rbusHandle_t handle, rbus_path_config_t const *cfg,
                         double interval,
                         char out_instances[][RBUS_PLUGIN_NAME_LEN],
                         int max_instances, int *out_count) {
  if (out_count != NULL)
    *out_count = 0;

  char wildcard[2 * RBUS_PLUGIN_NAME_LEN];
  int written = snprintf(wildcard, sizeof(wildcard), "%s*%s",
                          cfg->table_prefix, cfg->table_suffix);
  if (written < 0 || (size_t)written >= sizeof(wildcard)) {
    ERROR("rbus plugin: \"%s\": wildcard query path too long", cfg->path);
    return -1;
  }

  int num_props = 0;
  rbusProperty_t properties = NULL;
  char const *param_names[1] = {wildcard};
  rbusError_t rc =
      rbus_getExt(handle, 1, param_names, &num_props, &properties);
  if (rc != RBUS_ERROR_SUCCESS) {
    WARNING("rbus plugin: \"%s\": rbus_getExt(\"%s\") failed: %d", cfg->path,
            wildcard, (int)rc);
    return -1;
  }

  int dispatched = 0;
  rbusProperty_t prop = properties;
  for (int i = 0; i < num_props && prop != NULL; i++, prop = rbusProperty_GetNext(prop)) {
    char const *name = rbusProperty_GetName(prop);
    char instance[RBUS_PLUGIN_NAME_LEN];

    if (!extract_instance(name, cfg->table_prefix, cfg->table_suffix,
                           instance, sizeof(instance))) {
      WARNING("rbus plugin: \"%s\": could not derive a row instance from "
              "\"%s\"; skipping",
              cfg->path, name);
      continue;
    }

    if (out_instances != NULL && out_count != NULL && *out_count < max_instances) {
      rbus_strlcpy(out_instances[*out_count], instance, RBUS_PLUGIN_NAME_LEN);
      (*out_count)++;
    }

    rbusValue_t value = rbusProperty_GetValue(prop);
    if (rbus_dispatch_value(cfg, instance, interval, value))
      dispatched++;
  }

  // Releasing the head of the list cascades to every node linked via
  // rbusProperty_SetNext/Append, which rbus_getExt uses to chain results
  // (verified against the live bus; do not release each node individually).
  if (properties != NULL)
    rbusProperty_Release(properties);

  return dispatched;
}
