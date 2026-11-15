#!/usr/bin/env python3
"""Run actual plugin sources in a Luau VM with a small Noctalia host double."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--runner", required=True, help="Luau runner accepting a file; needs loadstring")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
plugin = root / "integrations/noctalia/umbriel-connect"

def literal(text):
    n = 0
    while "]" + "=" * n + "]" in text:
        n += 1
    return "[" + "=" * n + "[\n" + text + "]" + "=" * n + "]"

source = "local serviceSource = " + literal((plugin / "service.luau").read_text())
source += "\nlocal widgetSource = " + literal((plugin / "widget.luau").read_text())
source += r'''
local checks = 0
local function check(value, message)
  checks += 1
  assert(value, message)
end
local shared, watchers, states = {}, {}, {}
local stream, onLine, launched, rendered
local configured = "/tmp/a'b ; $(evil)/umbriel-connect"
noctalia = {
  getConfig = function(key) check(key == "binary", "setting"); return configured end,
  expandPath = function(s) return s end,
  runStream = function(cmd, callback) stream, onLine = cmd, callback end,
  runAsync = function(argv) launched = argv end,
  state = {
    set = function(key, value)
      shared[key] = value
      for _, cb in ipairs(watchers[key] or {}) do cb(value) end
    end,
    get = function(key) return shared[key] end,
    watch = function(key, cb)
      watchers[key] = watchers[key] or {}
      table.insert(watchers[key], cb)
    end,
  },
  json = { decode = function(line) return states[line] end },
}
barWidget = {
  setVisible = function(v) check(v == true, "widget remains visible") end,
  render = function(tree) rendered = tree end,
}
ui = {
  row = function(props, children) return {props = props, children = children} end,
  glyph = function(props) return {props = props} end,
}
assert(loadstring(serviceSource, "@service.luau"))()
check(stream:find("watch", 1, true) ~= nil, "event watcher")
check(stream:find("'\\''", 1, true) ~= nil, "POSIX quote for configured path")
check(stream:find("sleep 3", 1, true) ~= nil, "retry backoff")
check(stream:find('"available":false', 1, true) ~= nil, "clear on watcher exit")
check(shared.connection.available == false, "initial disconnected state")
assert(loadstring(widgetSource, "@widget.luau"))()
check(rendered.props.tooltip:find("no está disponible", 1, true) ~= nil, "unavailable tooltip")
states.empty = {available = true, devices = {}}
onLine("empty")
check(rendered.props.tooltip:find("Sin teléfonos", 1, true) ~= nil, "no connected devices")
states.connected = {available = true, devices = {
  {name = "Teléfono", paired = true, reachable = true, battery = 76},
  {name = "No confiable", paired = false, reachable = true, battery = 10},
  {name = "Lejano", paired = true, reachable = false, battery = 99},
}}
onLine("connected")
check(rendered.props.key == "phone-1", "only connected paired devices counted")
check(rendered.props.tooltip:find("76%", 1, true) ~= nil, "battery in tooltip")
check(rendered.props.tooltip:find("No confiable", 1, true) == nil, "unpaired device omitted")
check(rendered.props.tooltip:find("Lejano", 1, true) == nil, "unreachable device omitted")
rendered.props.onClick()
check(type(launched) == "table" and #launched == 1 and launched[1] == configured, "launch by argv")
local previous = shared.connection
onLine("malformed")
check(shared.connection == previous, "malformed records ignored")
states.bad = {available = true, devices = 5}
onLine("bad")
check(shared.connection == previous, "invalid device list ignored")
states.unknownBattery = {available = true, devices = {{name="Otro", paired=true, reachable=true, battery=-1}}}
onLine("unknownBattery")
check(rendered.props.tooltip:find("-1%", 1, true) == nil, "unknown battery omitted")
states.disconnected = {available = false, devices = {}}
onLine("disconnected")
check(rendered.props.key == "phone-0", "disconnect clears state")
print("Noctalia plugin: " .. checks .. " checks passed")
'''
with tempfile.TemporaryDirectory(prefix="umbriel-connect-luau-") as folder:
    bundle = Path(folder) / "plugin-test.luau"
    bundle.write_text(source)
    subprocess.run([args.runner, str(bundle)], check=True)
