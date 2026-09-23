// Entry point for the collectd `rbus` plugin: wires config parsing, the
// per-path polling reads, and the subscribe thread together. See design.md
// for the overall shape.
#include <collectd/core/daemon/plugin.h>
#include <collectd/liboconfig/oconfig.h>
#include <rbus.h>

#include "rbus_config.h"
#include "rbus_dispatch.h"
#include "rbus_subscribe.h"
#include "rbus_tables.h"
#include "rbus_types.h"

static rbus_path_config_t *g_configs = NULL;
static rbusHandle_t g_poll_handle = NULL;
static rbus_subscribe_ctx_t *g_subscribe_ctx = NULL;

static int rbus_plugin_config(oconfig_item_t *ci) {
  int parsed = rbus_config_parse_plugin(ci, &g_configs);
  INFO("rbus plugin: parsed %d valid <Path> block(s)", parsed);
  return 0;
}

static int rbus_plugin_read(user_data_t *ud) {
  rbus_path_config_t *cfg = (rbus_path_config_t *)ud->data;

  if (cfg->is_table) {
    rbus_tables_collect(g_poll_handle, cfg, cfg->interval, NULL, 0, NULL);
    return 0;
  }

  rbusValue_t value = NULL;
  rbusError_t rc = rbus_get(g_poll_handle, cfg->path, &value);
  if (rc != RBUS_ERROR_SUCCESS) {
    WARNING("rbus plugin: \"%s\": rbus_get failed: %d", cfg->path, (int)rc);
    return -1;
  }
  rbus_dispatch_value(cfg, NULL, cfg->interval, value);
  rbusValue_Release(value);
  return 0;
}

static int rbus_plugin_init(void) {
  rbus_dispatch_set_default_interval(CDTIME_T_TO_DOUBLE(plugin_get_interval()));

  bool any_poll = false;
  for (rbus_path_config_t *cfg = g_configs; cfg != NULL; cfg = cfg->next) {
    if (cfg->mode == RBUS_PLUGIN_MODE_POLL) {
      any_poll = true;
      break;
    }
  }

  if (any_poll) {
    rbusError_t rc = rbus_open(&g_poll_handle, "collectd-rbus-plugin-poll");
    if (rc != RBUS_ERROR_SUCCESS) {
      ERROR("rbus plugin: rbus_open (poll handle) failed: %d", (int)rc);
      return -1;
    }

    for (rbus_path_config_t *cfg = g_configs; cfg != NULL; cfg = cfg->next) {
      if (cfg->mode != RBUS_PLUGIN_MODE_POLL)
        continue;

      user_data_t ud = {.data = cfg, .free_func = NULL};
      int rc2 = plugin_register_complex_read(
          /* group = */ "rbus", /* name = */ cfg->path, rbus_plugin_read,
          DOUBLE_TO_CDTIME_T(cfg->interval), &ud);
      if (rc2 != 0)
        ERROR("rbus plugin: \"%s\": plugin_register_complex_read failed: %d",
              cfg->path, rc2);
    }
  }

  g_subscribe_ctx = rbus_subscribe_start(g_configs);

  return 0;
}

static int rbus_plugin_shutdown(void) {
  rbus_subscribe_stop(g_subscribe_ctx);
  g_subscribe_ctx = NULL;

  if (g_poll_handle != NULL) {
    rbus_close(g_poll_handle);
    g_poll_handle = NULL;
  }

  rbus_path_config_list_free(g_configs);
  g_configs = NULL;

  return 0;
}

void module_register(void) {
  plugin_register_complex_config("rbus", rbus_plugin_config);
  plugin_register_init("rbus", rbus_plugin_init);
  plugin_register_shutdown("rbus", rbus_plugin_shutdown);
}
