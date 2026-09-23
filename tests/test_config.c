// Unit tests for the config parser (tasks 2.1/2.2). Builds oconfig_item_t
// trees by hand -- no need to link liboconfig (it isn't shipped as a
// separate library) since we only exercise our own translation code, never
// oconfig_parse_file/oconfig_free.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rbus_config.h"

static oconfig_item_t leaf_str(char const *key, oconfig_value_t *val,
                                char const *str) {
  val->type = OCONFIG_TYPE_STRING;
  val->value.string = (char *)str;
  return (oconfig_item_t){.key = (char *)key,
                           .values = val,
                           .values_num = 1,
                           .children = NULL,
                           .children_num = 0};
}

static oconfig_item_t leaf_num(char const *key, oconfig_value_t *val,
                                double num) {
  val->type = OCONFIG_TYPE_NUMBER;
  val->value.number = num;
  return (oconfig_item_t){.key = (char *)key,
                           .values = val,
                           .values_num = 1,
                           .children = NULL,
                           .children_num = 0};
}

static void test_valid_scalar_poll_block(void) {
  oconfig_value_t path_val, mode_val, interval_val, type_val, vtype_val;
  oconfig_item_t children[4] = {
      leaf_str("Mode", &mode_val, "Poll"),
      leaf_num("Interval", &interval_val, 30.0),
      leaf_str("Type", &type_val, "gauge"),
      leaf_str("ValueType", &vtype_val, "gauge"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 4};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.DeviceInfo.UpTime";

  rbus_path_config_t out;
  char errbuf[256];
  assert(rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
  assert(strcmp(out.path, "Device.DeviceInfo.UpTime") == 0);
  assert(!out.is_table);
  assert(out.mode == RBUS_PLUGIN_MODE_POLL);
  assert(out.interval == 30.0);
  assert(strcmp(out.type, "gauge") == 0);
  assert(out.value_type == DS_TYPE_GAUGE);
}

static void test_valid_table_subscribe_block(void) {
  oconfig_value_t path_val, mode_val, type_val, vtype_val;
  oconfig_item_t children[3] = {
      leaf_str("Mode", &mode_val, "Subscribe"),
      leaf_str("Type", &type_val, "if_octets"),
      leaf_str("ValueType", &vtype_val, "counter"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 3};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.Bridging.Bridge.1.Port.{i}.Stats.BytesSent";

  rbus_path_config_t out;
  char errbuf[256];
  assert(rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
  assert(out.is_table);
  assert(strcmp(out.table_prefix, "Device.Bridging.Bridge.1.Port.") == 0);
  assert(strcmp(out.table_suffix, ".Stats.BytesSent") == 0);
  assert(out.mode == RBUS_PLUGIN_MODE_SUBSCRIBE);
  assert(out.value_type == DS_TYPE_COUNTER);
}

static void test_missing_mode_is_rejected(void) {
  oconfig_value_t path_val, type_val, vtype_val;
  oconfig_item_t children[2] = {
      leaf_str("Type", &type_val, "gauge"),
      leaf_str("ValueType", &vtype_val, "gauge"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 2};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.DeviceInfo.UpTime";

  rbus_path_config_t out;
  char errbuf[256];
  assert(!rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
}

static void test_both_modes_is_rejected(void) {
  oconfig_value_t path_val, mode1_val, mode2_val, interval_val, type_val,
      vtype_val;
  oconfig_item_t children[5] = {
      leaf_str("Mode", &mode1_val, "Poll"),
      leaf_str("Mode", &mode2_val, "Subscribe"),
      leaf_num("Interval", &interval_val, 30.0),
      leaf_str("Type", &type_val, "gauge"),
      leaf_str("ValueType", &vtype_val, "gauge"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 5};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.DeviceInfo.UpTime";

  rbus_path_config_t out;
  char errbuf[256];
  assert(!rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
}

static void test_poll_without_interval_is_rejected(void) {
  oconfig_value_t path_val, mode_val, type_val, vtype_val;
  oconfig_item_t children[3] = {
      leaf_str("Mode", &mode_val, "Poll"),
      leaf_str("Type", &type_val, "gauge"),
      leaf_str("ValueType", &vtype_val, "gauge"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 3};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.DeviceInfo.UpTime";

  rbus_path_config_t out;
  char errbuf[256];
  assert(!rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
}

static void test_multiple_placeholders_is_rejected(void) {
  oconfig_value_t path_val, mode_val, type_val, vtype_val;
  oconfig_item_t children[3] = {
      leaf_str("Mode", &mode_val, "Subscribe"),
      leaf_str("Type", &type_val, "gauge"),
      leaf_str("ValueType", &vtype_val, "gauge"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 3};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.X.{i}.Y.{i}.Z";

  rbus_path_config_t out;
  char errbuf[256];
  assert(!rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
}

static void test_unknown_key_is_rejected(void) {
  oconfig_value_t path_val, mode_val, type_val, vtype_val, bogus_val;
  oconfig_item_t children[4] = {
      leaf_str("Mode", &mode_val, "Subscribe"),
      leaf_str("Type", &type_val, "gauge"),
      leaf_str("ValueType", &vtype_val, "gauge"),
      leaf_str("Bogus", &bogus_val, "nope"),
  };
  oconfig_item_t path_block = {.key = "Path",
                               .values = &path_val,
                               .values_num = 1,
                               .children = children,
                               .children_num = 4};
  path_val.type = OCONFIG_TYPE_STRING;
  path_val.value.string = "Device.DeviceInfo.UpTime";

  rbus_path_config_t out;
  char errbuf[256];
  assert(!rbus_config_parse_path_block(&path_block, &out, errbuf, sizeof(errbuf)));
}

static void test_parse_plugin_skips_invalid_blocks(void) {
  oconfig_value_t good_path_val, good_mode_val, good_type_val, good_vtype_val;
  oconfig_item_t good_children[3] = {
      leaf_str("Mode", &good_mode_val, "Subscribe"),
      leaf_str("Type", &good_type_val, "gauge"),
      leaf_str("ValueType", &good_vtype_val, "gauge"),
  };
  oconfig_item_t good_block = {.key = "Path",
                                .values = &good_path_val,
                                .values_num = 1,
                                .children = good_children,
                                .children_num = 3};
  good_path_val.type = OCONFIG_TYPE_STRING;
  good_path_val.value.string = "Device.DeviceInfo.UpTime";

  oconfig_value_t bad_path_val;
  oconfig_item_t bad_block = {.key = "Path",
                              .values = &bad_path_val,
                              .values_num = 1,
                              .children = NULL,
                              .children_num = 0};
  bad_path_val.type = OCONFIG_TYPE_STRING;
  bad_path_val.value.string = "Device.Missing.Everything";

  oconfig_item_t plugin_children[2] = {good_block, bad_block};
  oconfig_item_t plugin_ci = {.key = "Plugin",
                              .values = NULL,
                              .values_num = 0,
                              .children = plugin_children,
                              .children_num = 2};

  rbus_path_config_t *head = NULL;
  int parsed = rbus_config_parse_plugin(&plugin_ci, &head);
  assert(parsed == 1);
  assert(head != NULL);
  assert(strcmp(head->path, "Device.DeviceInfo.UpTime") == 0);
  assert(head->next == NULL);

  rbus_path_config_list_free(head);
}

int main(void) {
  test_valid_scalar_poll_block();
  test_valid_table_subscribe_block();
  test_missing_mode_is_rejected();
  test_both_modes_is_rejected();
  test_poll_without_interval_is_rejected();
  test_multiple_placeholders_is_rejected();
  test_unknown_key_is_rejected();
  test_parse_plugin_skips_invalid_blocks();

  printf("all rbus_config tests passed\n");
  return 0;
}
