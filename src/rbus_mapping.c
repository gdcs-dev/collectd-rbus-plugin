#include "rbus_mapping.h"

// Extracts `rv` as a signed 64-bit integer. Returns false for non-integer
// rbus types (string, bytes, datetime, double/single, object/property).
static bool get_as_int64(rbusValue_t rv, int64_t *out) {
  switch (rbusValue_GetType(rv)) {
  case RBUS_BOOLEAN:
    *out = rbusValue_GetBoolean(rv) ? 1 : 0;
    return true;
  case RBUS_CHAR:
    *out = rbusValue_GetChar(rv);
    return true;
  case RBUS_BYTE:
    *out = rbusValue_GetByte(rv);
    return true;
  case RBUS_INT8:
    *out = rbusValue_GetInt8(rv);
    return true;
  case RBUS_UINT8:
    *out = rbusValue_GetUInt8(rv);
    return true;
  case RBUS_INT16:
    *out = rbusValue_GetInt16(rv);
    return true;
  case RBUS_UINT16:
    *out = rbusValue_GetUInt16(rv);
    return true;
  case RBUS_INT32:
    *out = rbusValue_GetInt32(rv);
    return true;
  case RBUS_UINT32:
    *out = rbusValue_GetUInt32(rv);
    return true;
  case RBUS_INT64:
    *out = rbusValue_GetInt64(rv);
    return true;
  case RBUS_UINT64: {
    uint64_t u = rbusValue_GetUInt64(rv);
    if (u > (uint64_t)INT64_MAX)
      return false;
    *out = (int64_t)u;
    return true;
  }
  default:
    return false;
  }
}

static bool get_as_double(rbusValue_t rv, double *out) {
  switch (rbusValue_GetType(rv)) {
  case RBUS_SINGLE:
    *out = rbusValue_GetSingle(rv);
    return true;
  case RBUS_DOUBLE:
    *out = rbusValue_GetDouble(rv);
    return true;
  default: {
    int64_t i;
    if (!get_as_int64(rv, &i))
      return false;
    *out = (double)i;
    return true;
  }
  }
}

bool rbus_mapping_convert(rbusValue_t rv, int value_type, value_t *out) {
  switch (value_type) {
  case DS_TYPE_GAUGE: {
    double d;
    if (!get_as_double(rv, &d))
      return false;
    out->gauge = (gauge_t)d;
    return true;
  }
  case DS_TYPE_COUNTER: {
    int64_t i;
    if (!get_as_int64(rv, &i) || i < 0)
      return false;
    out->counter = (counter_t)i;
    return true;
  }
  case DS_TYPE_DERIVE: {
    int64_t i;
    if (!get_as_int64(rv, &i))
      return false;
    out->derive = (derive_t)i;
    return true;
  }
  case DS_TYPE_ABSOLUTE: {
    int64_t i;
    if (!get_as_int64(rv, &i) || i < 0)
      return false;
    out->absolute = (absolute_t)i;
    return true;
  }
  default:
    return false;
  }
}
