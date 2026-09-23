// Unit tests for rbus_mapping_convert (task 3.2/3.3). Plain assert-based, no
// external test framework -- links against real librbus for rbusValue_t but
// stubs collectd's plugin_log so it doesn't need the collectd daemon.
#include <assert.h>
#include <rbus.h>
#include <stdio.h>

#include "rbus_mapping.h"

static void test_gauge_from_uint32(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetUInt32(v, 42);

  value_t out;
  assert(rbus_mapping_convert(v, DS_TYPE_GAUGE, &out));
  assert(out.gauge == 42.0);

  rbusValue_Release(v);
}

static void test_counter_from_uint32(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetUInt32(v, 208392);

  value_t out;
  assert(rbus_mapping_convert(v, DS_TYPE_COUNTER, &out));
  assert(out.counter == 208392ULL);

  rbusValue_Release(v);
}

static void test_derive_from_int32_negative(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetInt32(v, -7);

  value_t out;
  assert(rbus_mapping_convert(v, DS_TYPE_DERIVE, &out));
  assert(out.derive == -7);

  rbusValue_Release(v);
}

static void test_counter_rejects_negative(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetInt32(v, -1);

  value_t out;
  assert(!rbus_mapping_convert(v, DS_TYPE_COUNTER, &out));

  rbusValue_Release(v);
}

static void test_absolute_from_uint64(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetUInt64(v, 123456789ULL);

  value_t out;
  assert(rbus_mapping_convert(v, DS_TYPE_ABSOLUTE, &out));
  assert(out.absolute == 123456789ULL);

  rbusValue_Release(v);
}

static void test_gauge_from_double(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetDouble(v, 3.5);

  value_t out;
  assert(rbus_mapping_convert(v, DS_TYPE_GAUGE, &out));
  assert(out.gauge == 3.5);

  rbusValue_Release(v);
}

static void test_string_is_incompatible_with_every_numeric_type(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetString(v, "not a number");

  value_t out;
  assert(!rbus_mapping_convert(v, DS_TYPE_GAUGE, &out));
  assert(!rbus_mapping_convert(v, DS_TYPE_COUNTER, &out));
  assert(!rbus_mapping_convert(v, DS_TYPE_DERIVE, &out));
  assert(!rbus_mapping_convert(v, DS_TYPE_ABSOLUTE, &out));

  rbusValue_Release(v);
}

static void test_boolean_maps_to_gauge_zero_or_one(void) {
  rbusValue_t v = NULL;
  rbusValue_Init(&v);
  rbusValue_SetBoolean(v, true);

  value_t out;
  assert(rbus_mapping_convert(v, DS_TYPE_GAUGE, &out));
  assert(out.gauge == 1.0);

  rbusValue_Release(v);
}

int main(void) {
  test_gauge_from_uint32();
  test_counter_from_uint32();
  test_derive_from_int32_negative();
  test_counter_rejects_negative();
  test_absolute_from_uint64();
  test_gauge_from_double();
  test_string_is_incompatible_with_every_numeric_type();
  test_boolean_maps_to_gauge_zero_or_one();

  printf("all rbus_mapping tests passed\n");
  return 0;
}
