// Standalone unit tests link this instead of the collectd daemon. Plugin
// code calls ERROR()/WARNING()/etc. (macros around plugin_log) even in paths
// exercised by these tests, so give them a trivial stderr-based definition
// rather than requiring a full collectd process just to run a unit test.
#include <collectd/core/daemon/plugin.h>
#include <stdarg.h>
#include <stdio.h>

void plugin_log(int severity, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "[%d] ", severity);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}

cdtime_t plugin_get_interval(void) { return DOUBLE_TO_CDTIME_T(10.0); }
