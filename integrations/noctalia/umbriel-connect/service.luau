--!nonstrict
-- One C++ process listens to D-Bus. No periodic device queries.
local function quote(s)
  return "'" .. string.gsub(s, "'", "'\\''") .. "'"
end

local function start()
  local binary = noctalia.expandPath(noctalia.getConfig("binary") or "~/.local/bin/umbriel-connect")
  noctalia.state.set("connection", { available = false, devices = {} })
  -- runStream uses a shell; quote the configured path as a single argument.
  -- Retry only after a process exit; the host terminates this stream on unload.
  noctalia.runStream("while true; do " .. quote(binary) .. " watch; printf '%s\\n' '{\"available\":false,\"devices\":[]}'; sleep 3; done", function(line)
    local state = noctalia.json.decode(line)
    if type(state) == "table" and type(state.devices) == "table" then
      noctalia.state.set("connection", state)
    end
  end)
end

start()
