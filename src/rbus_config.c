#include "rbus_config.h"

#include <collectd/core/daemon/plugin.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "rbus_types.h"

#define TABLE_PLACEHOLDER "{i}"

void rbus_path_config_list_free(rbus_path_config_t *head) {
  while (head != NULL) {
    rbus_path_config_t *next = head->next;
    free(head);
    head = next;
  }
}

void rbus_strlcpy(char *dest, char const *src, size_t dest_len) {
  if (dest_len == 0)
    return;
  size_t i = 0;
  for (; i + 1 < dest_len && src[i] != '\0'; i++)
    dest[i] = src[i];
  dest[i] = '\0';
}

static bool split_table_path(char const *path, rbus_path_config_t *out,
                              char *errbuf, size_t errbuf_len) {
  char const *first = strstr(path, TABLE_PLACEHOLDER);
  if (first == NULL) {
    out->is_table = false;
    out->table_prefix[0] = '\0';
    out->table_suffix[0] = '\0';
    return true;
  }

  char const *second = strstr(first + strlen(TABLE_PLACEHOLDER), TABLE_PLACEHOLDER);
  if (second != NULL) {
    snprintf(errbuf, errbuf_len,
             "path \"%s\" has more than one \"%s\" placeholder", path,
             TABLE_PLACEHOLDER);
    return false;
  }

  size_t prefix_len = (size_t)(first - path);
  char const *suffix = first + strlen(TABLE_PLACEHOLDER);
  if (prefix_len >= sizeof(out->table_prefix) ||
      strlen(suffix) >= sizeof(out->table_suffix)) {
    snprintf(errbuf, errbuf_len, "path \"%s\" is too long", path);
    return false;
  }

  memcpy(out->table_prefix, path, prefix_len);
  out->table_prefix[prefix_len] = '\0';
  strncpy(out->table_suffix, suffix, sizeof(out->table_suffix) - 1);
  out->table_suffix[sizeof(out->table_suffix) - 1] = '\0';
  out->is_table = true;
  return true;
}

static bool parse_mode(char const *s, rbus_plugin_mode_t *out) {
  if (strcasecmp(s, "poll") == 0) {
    *out = RBUS_PLUGIN_MODE_POLL;
    return true;
  }
  if (strcasecmp(s, "subscribe") == 0) {
    *out = RBUS_PLUGIN_MODE_SUBSCRIBE;
    return true;
  }
  return false;
}

static bool parse_value_type(char const *s, int *out) {
  if (strcasecmp(s, "gauge") == 0) {
    *out = DS_TYPE_GAUGE;
    return true;
  }
  if (strcasecmp(s, "counter") == 0) {
    *out = DS_TYPE_COUNTER;
    return true;
  }
  if (strcasecmp(s, "derive") == 0) {
    *out = DS_TYPE_DERIVE;
    return true;
  }
  if (strcasecmp(s, "absolute") == 0) {
    *out = DS_TYPE_ABSOLUTE;
    return true;
  }
  return false;
}

bool rbus_config_parse_path_block(oconfig_item_t *path_block,
                                   rbus_path_config_t *out, char *errbuf,
                                   size_t errbuf_len) {
  memset(out, 0, sizeof(*out));

  if (path_block->values_num != 1 ||
      path_block->values[0].type != OCONFIG_TYPE_STRING) {
    snprintf(errbuf, errbuf_len,
             "<Path> requires exactly one string argument (the rbus path)");
    return false;
  }
  if (strlen(path_block->values[0].value.string) >= sizeof(out->path)) {
    snprintf(errbuf, errbuf_len, "path is too long");
    return false;
  }
  strncpy(out->path, path_block->values[0].value.string, sizeof(out->path) - 1);

  bool have_mode = false;
  bool have_type = false;
  bool have_value_type = false;
  bool poll_seen = false;
  bool subscribe_seen = false;

  for (int i = 0; i < path_block->children_num; i++) {
    oconfig_item_t *child = &path_block->children[i];

    if (strcasecmp(child->key, "Mode") == 0) {
      if (child->values_num != 1 ||
          child->values[0].type != OCONFIG_TYPE_STRING) {
        snprintf(errbuf, errbuf_len, "\"%s\": Mode requires a string value",
                 out->path);
        return false;
      }
      rbus_plugin_mode_t mode;
      if (!parse_mode(child->values[0].value.string, &mode)) {
        snprintf(errbuf, errbuf_len,
                 "\"%s\": Mode must be \"Poll\" or \"Subscribe\", got \"%s\"",
                 out->path, child->values[0].value.string);
        return false;
      }
      if (mode == RBUS_PLUGIN_MODE_POLL)
        poll_seen = true;
      else
        subscribe_seen = true;
      out->mode = mode;
      have_mode = true;
    } else if (strcasecmp(child->key, "Interval") == 0) {
      if (child->values_num != 1 ||
          child->values[0].type != OCONFIG_TYPE_NUMBER) {
        snprintf(errbuf, errbuf_len,
                 "\"%s\": Interval requires a numeric value", out->path);
        return false;
      }
      out->interval = child->values[0].value.number;
    } else if (strcasecmp(child->key, "Type") == 0) {
      if (child->values_num != 1 ||
          child->values[0].type != OCONFIG_TYPE_STRING) {
        snprintf(errbuf, errbuf_len, "\"%s\": Type requires a string value",
                 out->path);
        return false;
      }
      if (strlen(child->values[0].value.string) >= sizeof(out->type)) {
        snprintf(errbuf, errbuf_len, "\"%s\": Type is too long", out->path);
        return false;
      }
      strncpy(out->type, child->values[0].value.string, sizeof(out->type) - 1);
      have_type = true;
    } else if (strcasecmp(child->key, "ValueType") == 0) {
      if (child->values_num != 1 ||
          child->values[0].type != OCONFIG_TYPE_STRING) {
        snprintf(errbuf, errbuf_len,
                 "\"%s\": ValueType requires a string value", out->path);
        return false;
      }
      if (!parse_value_type(child->values[0].value.string, &out->value_type)) {
        snprintf(errbuf, errbuf_len,
                 "\"%s\": ValueType must be one of gauge/counter/derive/"
                 "absolute, got \"%s\"",
                 out->path, child->values[0].value.string);
        return false;
      }
      have_value_type = true;
    } else {
      snprintf(errbuf, errbuf_len, "\"%s\": unknown option \"%s\"", out->path,
               child->key);
      return false;
    }
  }

  // Per the "Per-path acquisition mode" requirement: a block declaring no
  // mode, or both poll and subscribe, is rejected outright (no silent
  // default).
  if (!have_mode || (poll_seen && subscribe_seen)) {
    snprintf(errbuf, errbuf_len,
             "\"%s\": must declare exactly one Mode (Poll or Subscribe)",
             out->path);
    return false;
  }
  if (out->mode == RBUS_PLUGIN_MODE_POLL && out->interval <= 0.0) {
    snprintf(errbuf, errbuf_len,
             "\"%s\": Poll mode requires a positive Interval", out->path);
    return false;
  }
  if (!have_type || !have_value_type) {
    snprintf(errbuf, errbuf_len, "\"%s\": Type and ValueType are required",
             out->path);
    return false;
  }

  return split_table_path(out->path, out, errbuf, errbuf_len);
}

int rbus_config_parse_plugin(oconfig_item_t *ci, rbus_path_config_t **out_head) {
  int parsed = 0;

  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = &ci->children[i];

    if (strcasecmp(child->key, "Path") != 0) {
      WARNING("rbus plugin: ignoring unknown top-level config key \"%s\"",
              child->key);
      continue;
    }

    rbus_path_config_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL) {
      ERROR("rbus plugin: calloc failed while parsing config");
      continue;
    }

    char errbuf[256];
    if (!rbus_config_parse_path_block(child, entry, errbuf, sizeof(errbuf))) {
      ERROR("rbus plugin: rejecting <Path> block: %s", errbuf);
      free(entry);
      continue;
    }

    entry->next = *out_head;
    *out_head = entry;
    parsed++;
  }

  return parsed;
}
