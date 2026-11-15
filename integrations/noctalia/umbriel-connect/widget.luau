--!nonstrict
local function open()
  local binary = noctalia.expandPath(noctalia.getConfig("binary") or "~/.local/bin/umbriel-connect")
  noctalia.runAsync({ binary })
end

local function render(state)
  state = state or {}
  local lines = { "Umbriel Connect" }
  local count = 0
  for _, device in ipairs(state.devices or {}) do
    if device.paired and device.reachable then
      count += 1
      local battery = tonumber(device.battery) or -1
      table.insert(lines, tostring(device.name) .. (battery >= 0 and (" · " .. battery .. "%") or ""))
    end
  end
  if not state.available then
    table.insert(lines, "KDE Connect no está disponible")
  elseif count == 0 then
    table.insert(lines, "Sin teléfonos conectados")
  end
  table.insert(lines, "Presiona para abrir")
  barWidget.setVisible(true)
  barWidget.render(ui.row({ key = "phone-" .. count, align = "center", gap = 4,
    tooltip = table.concat(lines, "\n"), onClick = open }, {
    ui.glyph({ name = count > 0 and "device-mobile" or "device-mobile-off" }),
  }))
end

noctalia.state.watch("connection", render)
render(noctalia.state.get("connection"))
