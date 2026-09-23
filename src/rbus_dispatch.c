#include "rbus_dispatch.h"

#include "rbus_mapping.h"

// Defaults to collectd's own historical default (10s) until init runs.
static double g_default_interval_seconds = 10.0;

void rbus_dispatch_set_default_interval(double seconds) {
  if (seconds > 0.0)
    g_default_interval_seconds = seconds;
}

bool rbus_dispatch_value(rbus_path_config_t const *cfg,
                          char const *plugin_instance, double interval,
                          rbusValue_t rv) {
  value_t value;
  if (!rbus_mapping_convert(rv, cfg->value_type, &value)) {
    ERROR("rbus plugin: \"%s\": rbus value type %d cannot be represented as "
          "the configured collectd value-type; discarding observation",
          cfg->path, (int)rbusValue_GetType(rv));
    return false;
  }

  value_list_t vl = {0};
  vl.values = &value;
  vl.values_len = 1;
  vl.time = 0; // collectd fills in "now"
  vl.interval = DOUBLE_TO_CDTIME_T(interval > 0.0 ? interval : g_default_interval_seconds);

  rbus_strlcpy(vl.plugin, "rbus", sizeof(vl.plugin));
  rbus_strlcpy(vl.type, cfg->type, sizeof(vl.type));
  if (plugin_instance != NULL)
    rbus_strlcpy(vl.plugin_instance, plugin_instance, sizeof(vl.plugin_instance));

  if (plugin_dispatch_values(&vl) != 0) {
    ERROR("rbus plugin: \"%s\": plugin_dispatch_values failed", cfg->path);
    return false;
  }
  return true;
}
