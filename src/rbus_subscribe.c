#include "rbus_subscribe.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rbus.h>

#include "rbus_dispatch.h"
#include "rbus_tables.h"

// Bounds one wildcard discovery pass; a table with more live rows than this
// is treated as a config problem, not a reason to allocate unboundedly.
#define MAX_TRACKED_ROWS 256

typedef struct tracked_row {
  char instance[RBUS_PLUGIN_NAME_LEN];
  char full_path[3 * RBUS_PLUGIN_NAME_LEN];
  rbus_path_config_t *cfg;
  struct tracked_row *next;
} tracked_row_t;

typedef struct table_state {
  rbus_path_config_t *cfg;
  tracked_row_t *rows;
  struct table_state *next;
} table_state_t;

struct rbus_subscribe_ctx {
  rbusHandle_t handle;
  pthread_t thread;
  rbus_path_config_t *configs; // not owned by this context
  table_state_t *tables;
};

static void on_scalar_value_changed(rbusHandle_t handle,
                                     rbusEvent_t const *eventData,
                                     rbusEventSubscription_t *subscription) {
  (void)eventData;
  rbus_path_config_t *cfg = (rbus_path_config_t *)subscription->userData;

  rbusValue_t value = NULL;
  rbusError_t rc = rbus_get(handle, cfg->path, &value);
  if (rc != RBUS_ERROR_SUCCESS) {
    WARNING("rbus plugin: \"%s\": rbus_get after subscribe event failed: %d",
            cfg->path, (int)rc);
    return;
  }
  rbus_dispatch_value(cfg, NULL, 0, value);
  rbusValue_Release(value);
}

static void on_row_value_changed(rbusHandle_t handle,
                                  rbusEvent_t const *eventData,
                                  rbusEventSubscription_t *subscription) {
  (void)eventData;
  tracked_row_t *row = (tracked_row_t *)subscription->userData;

  rbusValue_t value = NULL;
  rbusError_t rc = rbus_get(handle, row->full_path, &value);
  if (rc != RBUS_ERROR_SUCCESS) {
    WARNING("rbus plugin: \"%s\": rbus_get after subscribe event failed: %d",
            row->full_path, (int)rc);
    return;
  }
  rbus_dispatch_value(row->cfg, row->instance, 0, value);
  rbusValue_Release(value);
}

// Re-discovers a table's current rows and reconciles per-row VALUE_CHANGED
// subscriptions to match: newly discovered rows are subscribed and get an
// immediate dispatch (via rbus_tables_collect); rows no longer present are
// unsubscribed and dropped, per the "Table row discovery" requirement.
static void resync_table_rows(rbusHandle_t handle, table_state_t *ts) {
  char discovered[MAX_TRACKED_ROWS][RBUS_PLUGIN_NAME_LEN];
  int discovered_count = 0;

  rbus_tables_collect(handle, ts->cfg, 0, discovered, MAX_TRACKED_ROWS,
                      &discovered_count);

  for (int i = 0; i < discovered_count; i++) {
    bool already_tracked = false;
    for (tracked_row_t *r = ts->rows; r != NULL; r = r->next) {
      if (strcmp(r->instance, discovered[i]) == 0) {
        already_tracked = true;
        break;
      }
    }
    if (already_tracked)
      continue;

    tracked_row_t *row = calloc(1, sizeof(*row));
    if (row == NULL) {
      ERROR("rbus plugin: calloc failed while tracking a new row");
      continue;
    }
    rbus_strlcpy(row->instance, discovered[i], sizeof(row->instance));
    snprintf(row->full_path, sizeof(row->full_path), "%s%s%s",
             ts->cfg->table_prefix, discovered[i], ts->cfg->table_suffix);
    row->cfg = ts->cfg;

    rbusError_t rc = rbusEvent_Subscribe(handle, row->full_path,
                                          on_row_value_changed, row, 0);
    if (rc != RBUS_ERROR_SUCCESS) {
      WARNING("rbus plugin: \"%s\": failed to subscribe to row \"%s\": %d",
              ts->cfg->path, row->full_path, (int)rc);
      free(row);
      continue;
    }

    row->next = ts->rows;
    ts->rows = row;
  }

  tracked_row_t **link = &ts->rows;
  while (*link != NULL) {
    tracked_row_t *row = *link;
    bool still_present = false;
    for (int i = 0; i < discovered_count; i++) {
      if (strcmp(row->instance, discovered[i]) == 0) {
        still_present = true;
        break;
      }
    }
    if (still_present) {
      link = &row->next;
      continue;
    }
    rbusEvent_Unsubscribe(handle, row->full_path);
    *link = row->next;
    free(row);
  }
}

static void
on_table_membership_changed(rbusHandle_t handle, rbusEvent_t const *eventData,
                             rbusEventSubscription_t *subscription) {
  (void)eventData;
  table_state_t *ts = (table_state_t *)subscription->userData;
  resync_table_rows(handle, ts);
}

static void *subscribe_thread_main(void *arg) {
  rbus_subscribe_ctx_t *ctx = (rbus_subscribe_ctx_t *)arg;

  for (rbus_path_config_t *cfg = ctx->configs; cfg != NULL; cfg = cfg->next) {
    if (cfg->mode != RBUS_PLUGIN_MODE_SUBSCRIBE)
      continue;

    if (!cfg->is_table) {
      rbusError_t rc = rbusEvent_Subscribe(ctx->handle, cfg->path,
                                            on_scalar_value_changed, cfg, 0);
      if (rc != RBUS_ERROR_SUCCESS)
        WARNING("rbus plugin: \"%s\": rbusEvent_Subscribe failed: %d",
                cfg->path, (int)rc);
      continue;
    }

    table_state_t *ts = calloc(1, sizeof(*ts));
    if (ts == NULL) {
      ERROR("rbus plugin: calloc failed for table state");
      continue;
    }
    ts->cfg = cfg;
    ts->next = ctx->tables;
    ctx->tables = ts;

    // Seed initial rows/values before subscribing to membership changes, so
    // a table with existing rows is collected immediately (no restart
    // needed), per the "Plugin starts with existing rows" scenario.
    resync_table_rows(ctx->handle, ts);

    rbusError_t rc = rbusEvent_Subscribe(
        ctx->handle, cfg->table_prefix, on_table_membership_changed, ts, 0);
    if (rc != RBUS_ERROR_SUCCESS)
      WARNING("rbus plugin: \"%s\": failed to subscribe to table membership "
              "events on \"%s\": %d",
              cfg->path, cfg->table_prefix, (int)rc);
  }

  return NULL;
}

rbus_subscribe_ctx_t *rbus_subscribe_start(rbus_path_config_t *configs) {
  bool any_subscribe = false;
  for (rbus_path_config_t *cfg = configs; cfg != NULL; cfg = cfg->next) {
    if (cfg->mode == RBUS_PLUGIN_MODE_SUBSCRIBE) {
      any_subscribe = true;
      break;
    }
  }
  if (!any_subscribe)
    return NULL;

  rbus_subscribe_ctx_t *ctx = calloc(1, sizeof(*ctx));
  if (ctx == NULL) {
    ERROR("rbus plugin: calloc failed for subscribe context");
    return NULL;
  }
  ctx->configs = configs;

  rbusError_t rc = rbus_open(&ctx->handle, "collectd-rbus-plugin-subscribe");
  if (rc != RBUS_ERROR_SUCCESS) {
    ERROR("rbus plugin: rbus_open (subscribe handle) failed: %d", (int)rc);
    free(ctx);
    return NULL;
  }

  int perr = pthread_create(&ctx->thread, NULL, subscribe_thread_main, ctx);
  if (perr != 0) {
    ERROR("rbus plugin: pthread_create for subscribe thread failed: %d",
          perr);
    rbus_close(ctx->handle);
    free(ctx);
    return NULL;
  }

  return ctx;
}

void rbus_subscribe_stop(rbus_subscribe_ctx_t *ctx) {
  if (ctx == NULL)
    return;

  pthread_join(ctx->thread, NULL);

  for (rbus_path_config_t *cfg = ctx->configs; cfg != NULL; cfg = cfg->next) {
    if (cfg->mode == RBUS_PLUGIN_MODE_SUBSCRIBE && !cfg->is_table)
      rbusEvent_Unsubscribe(ctx->handle, cfg->path);
  }

  table_state_t *ts = ctx->tables;
  while (ts != NULL) {
    table_state_t *next_ts = ts->next;
    rbusEvent_Unsubscribe(ctx->handle, ts->cfg->table_prefix);

    tracked_row_t *row = ts->rows;
    while (row != NULL) {
      tracked_row_t *next_row = row->next;
      rbusEvent_Unsubscribe(ctx->handle, row->full_path);
      free(row);
      row = next_row;
    }
    free(ts);
    ts = next_ts;
  }

  rbus_close(ctx->handle);
  free(ctx);
}
