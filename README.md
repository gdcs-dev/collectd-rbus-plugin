# collectd-rbus-plugin

A collectd module that reads TR-181 data-model values from `rbus` and hands them to collectd
core for local storage (typically via collectd's stock `rrdtool` write plugin). This path is
independent of the OTLP pipeline: it does not touch `otel-relay`, `telemetry_agent`, or any cloud
delivery path. See `openspec/changes/add-collectd-rbus-plugin/` (proposal/design/specs) for the
full rationale.

## Build

Requires `librbus`/`librbuscore` + headers (the `rbus`/`rbus-elements` packages already provide
these) and `collectd-dev` (for the plugin API headers).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces `build/rbus.so` -- collectd's plugin loader expects `<name>.so` with no `lib`
prefix, so install it as `<collectd PluginDir>/rbus.so` (typically `/usr/lib/collectd/rbus.so`).
Also install `conf/types.db.rdk` somewhere collectd can load it (e.g.
`/usr/share/collectd-rbus-plugin/types.db.rdk`) and reference it with a second `TypesDB` directive
alongside the stock `types.db`.

### Tests

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

`test_rbus_config` and `test_rbus_mapping` are plain-assert unit tests with no collectd daemon or
live rbus bus dependency (they link a small `plugin_log` stub). There is no automated integration
test target; see "Manual integration check" below for verifying end-to-end behavior against a
live rbus bus.

## Config

```
LoadPlugin rbus
<Plugin rbus>
   <Path "Device.DeviceInfo.UpTime">
      Mode "Poll"
      Interval 60
      Type "uptime"
      ValueType "gauge"
   </Path>

   <Path "Device.Bridging.Bridge.1.Port.1.Enable">
      Mode "Subscribe"
      Type "rdk_bridge_port_enabled"
      ValueType "gauge"
   </Path>

   <Path "Device.Bridging.Bridge.1.Port.{i}.Stats.BytesSent">
      Mode "Poll"
      Interval 60
      Type "rdk_bridge_port_bytes"
      ValueType "counter"
   </Path>
</Plugin>
```

Each `<Path>` block declares:

- The rbus path. A path containing exactly one `{i}` placeholder is treated as a table row
  pattern; the plugin discovers current row instances from rbus itself (no static index list).
- `Mode`: `Poll` (re-reads on `Interval` seconds) or `Subscribe` (dispatches immediately on an
  rbus value-change event, or -- for a table path -- on a row added/removed event). Exactly one
  is required; a block declaring neither or both is rejected at config-load time and skipped
  (the rest of the config still loads).
- `Interval`: required for `Poll` mode, seconds between reads.
- `Type` / `ValueType`: the collectd `Type` name (must resolve against a loaded `types.db`) and
  its value kind (`gauge`/`counter`/`derive`/`absolute`). A value collectd's declared `ValueType`
  cannot represent (e.g. a negative number for `counter`) is discarded, not dispatched.

To add a new collected path: add a `<Path>` block. If it needs a `Type` shape not already in
collectd's stock `types.db` (e.g. a bounded percentage, or a bare signal-level gauge), add it to
`conf/types.db.rdk` first.

## Design notes

- **Poll** paths are driven by collectd's own `complex_read` scheduler on the interval given in
  config.
- **Subscribe** paths are handled by a dedicated background thread (started at plugin init) that
  opens its own rbus handle so `rbusEvent_Subscribe` callbacks can dispatch directly via
  `plugin_dispatch_values()` (documented safe from any thread) without a queue.
- **Table row discovery** uses rbus's own wildcard query (replacing `{i}` with `*` in a single
  `rbus_getExt` call) rather than a leaf-by-leaf enumeration. Poll-mode tables simply re-run this
  query every interval. Subscribe-mode tables re-run it whenever rbus reports a row added or
  removed on that table, then reconcile per-row `VALUE_CHANGED` subscriptions against the new row
  set.

See `design.md` in the OpenSpec change for the full rationale and alternatives considered.

## Manual integration check

Against a live `rbus` bus (e.g. inside the gateway container, where `rbus_elements` is already
running):

```bash
cp build/rbus.so /usr/lib/collectd/rbus.so
collectd -C /path/to/collectd.conf -f    # foreground, logs per the config's LoadPlugin logfile/syslog
```

Then, in another shell, use `rbuscli` to exercise the config live, e.g.:

```bash
rbuscli setvalues Device.Bridging.Bridge.1.Port.1.Enable boolean false   # fires a Subscribe-mode dispatch
rbuscli addrow "Device.Bridging.Bridge.1.Port."                          # a Subscribe-mode table picks up the new row without a restart
rbuscli delrow "Device.Bridging.Bridge.1.Port.N."                        # and stops updating a removed row
```

and inspect the configured write plugin's output (e.g. `.rrd` files under the `rrdtool` plugin's
`DataDir`, one per `Plugin`+`PluginInstance`+`Type` combination collectd saw).
