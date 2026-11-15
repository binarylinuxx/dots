-- Material 3-inspired desktop shell. See material-shell-design.md for sources.
local directory = debug.getinfo(1, "S").source:match("^@(.*/)") or "examples/"
package.path = directory .. "?.lua;" .. package.path
local Material = require("material-ui")
local base = require("vw.base")
local surface = require("vw.surface")
local window = require("vw.toplevel") -- close() also requests this runtime's exit
local animation = require("vw.animation")
local datetime = require("vw.datetime")
local apps = require("vw.services.applications")
local mpris = require("vw.services.mpris")
local audio = require("vw.services.pipewire")
local notifications = require("vw.services.notifications")
local zwwm = require("vw.services.zwwm")
local hypr = require("vw.services.hyprland")

local function safe(fn, fallback, ...)
  local ok, value = pcall(fn, ...)
  return ok and value or fallback
end
local function read(path)
  local file = io.open(path, "r")
  if not file then return "" end
  local result = file:read("*a") or ""; file:close(); return result
end
local function quote(value) return "'" .. tostring(value):gsub("'", "'\\''") .. "'" end
local function spawn(command) os.execute(command .. " >/dev/null 2>&1 &") end
local state_path = os.getenv("VW_MATERIAL_STATE") or
    ((os.getenv("XDG_RUNTIME_DIR") or "/tmp") .. "/vw-material-shell-state")
local lines = {}
local stored = read(state_path)
for line in (stored .. "\n"):gmatch("(.-)\n") do lines[#lines + 1] = line end
local state = {view = stored == "" and "home" or (lines[1] or "home"), query = lines[2] or "",
               theme = os.getenv("VW_MATERIAL_THEME") or lines[3] or "dark",
               drag = lines[4] or "", drag_value = tonumber(lines[5]) or 0,
               last = lines[6] or "home"}
local function save()
  local file = io.open(state_path .. ".tmp", "w")
  if not file then return end
  file:write(state.view, "\n", state.query:gsub("[\r\n]", " "), "\n", state.theme,
             "\n", state.drag, "\n", string.format("%.6f", state.drag_value), "\n", state.last, "\n")
  file:close(); os.rename(state_path .. ".tmp", state_path)
end
local function view(name)
  state.last = name; state.view = state.view == name and "" or name; state.drag = ""; save()
end
local function close_panel()
  if state.view ~= "" then state.last = state.view end
  state.view = ""; state.drag = ""; save()
end
local ui = Material.new(state.theme)
local C = ui.colors
local native = safe(zwwm.available, false)
local is_hypr = not native and safe(hypr.available, false)
local monitor
if native then monitor = safe(zwwm.outputs, {})[1]
elseif is_hypr then
  for _, entry in ipairs(safe(hypr.monitors, {})) do if not monitor or entry.focused then monitor = entry end end
end
monitor = monitor or {}
local W = tonumber(os.getenv("VW_MATERIAL_WIDTH")) or monitor.logical_width or
          math.floor((monitor.width or 1920) / (monitor.scale or 1))
local H = tonumber(os.getenv("VW_MATERIAL_HEIGHT")) or monitor.logical_height or
          math.floor((monitor.height or 1080) / (monitor.scale or 1))
assert(W >= 900 and H >= 720, "This Material shell targets desktop areas of at least 900×720")
local tags = native and safe(zwwm.tags, {}) or {}
if is_hypr then
  local current = safe(hypr.active_workspace, {id = 1}).id
  for id = 1, 6 do tags[#tags + 1] = {id = id, name = tostring(id), active = id == current} end
end
local player = safe(mpris.state, {available = false, playback_status = "Stopped"})
local output = safe(audio.default_sink, nil)
local input = safe(audio.default_source, nil)
local notification_error = ""
if os.getenv("VW_MATERIAL_NOTIFICATIONS") == "1" then
  local started, error = notifications.start()
  if not started then notification_error = error end
end
local own_notifications = notifications.running()
local alerts = own_notifications and notifications.history() or {}
local meminfo = read("/proc/meminfo")
local total = tonumber(meminfo:match("MemTotal:%s+(%d+)")) or 1
local free = tonumber(meminfo:match("MemAvailable:%s+(%d+)")) or total
local memory = (total - free) / total
local load = tonumber(read("/proc/loadavg"):match("^([%d.]+)")) or 0
local uptime = tonumber(read("/proc/uptime"):match("^([%d.]+)")) or 0

local reveal = animation.value("material-panel", state.view ~= "" and 1 or 0,
  {initial = 0, duration = 0.3, bezier = {0.2, 0, 0, 1}, min = 0, max = 1})
local display_view = state.view ~= "" and state.view or state.last
local page_alpha = 0
for _, name in ipairs({"home", "apps", "media", "controls", "alerts", "session"}) do
  local opacity = animation.value("material-page-" .. name, state.view == name and 1 or 0,
    {initial = 0, duration = 0.2, bezier = {0.2, 0, 0, 1}, min = 0, max = 1})
  if name == display_view then page_alpha = opacity end
end
local px, py = 108 + 20 * (1 - reveal), 84
local pw, ph = math.min(820, W - 124), math.min(580, H - 108)

local destinations = {{"home", "space_dashboard", "Home"}, {"apps", "apps", "Apps"},
                      {"media", "music_note", "Media"}, {"controls", "tune", "Controls"},
                      {"alerts", "notifications", "Alerts"}}
local selected = 0
for index, entry in ipairs(destinations) do if state.view == entry[1] then selected = index end end
local rail_x, rail_y, rail_width = 12, 12, 80
local top_x, top_y, top_width = 108, 12, W - 124
local regions = {{0, 0, 92, H}, {0, 0, W, 12}, {W - 12, 0, 12, H},
                 {0, H - 12, W, 12}, {top_x, top_y, top_width, 56}}
local members = {{x = top_x, y = top_y, width = top_width, height = 56, radius = 28}}
if reveal > 0.001 then members[#members + 1] = {x = px, y = py, width = pw,
  height = math.max(1, ph * reveal), radius = 28} end
-- Screen frame and rail, then each major panel as a rounded surface.
for _, edge in ipairs({{0, 0, 92, H}, {0, 0, W, 12}, {W - 12, 0, 12, H}, {0, H - 12, W, 12}}) do
  base.rectangle({x = edge[1], y = edge[2], width = edge[3], height = edge[4], color = C.container})
end
for _, member in ipairs(members) do
  base.rectangle({x = member.x, y = member.y, width = member.width, height = member.height,
    radius = member.radius, color = C.container})
end
-- A single primary FAB starts the app-launching flow.
ui:rect(24, 28, 56, 56, C.primary_container, 16, function() view("apps") end)
ui:icon("search", 24, 28, 56, 56, C.on_primary_container)
local indicator_y = animation.value("material-nav-indicator", 116 + math.max(0, selected - 1) * 72,
  {duration = 0.25, bezier = {0.2, 0, 0, 1}})
if selected > 0 then ui:rect(24, indicator_y, 56, 32, C.secondary_container, 16) end
for index, entry in ipairs(destinations) do
  local y = 116 + (index - 1) * 72
  ui:icon(entry[2], 24, y, 56, 32, index == selected and C.on_secondary_container or C.on_surface_variant)
  base.stack({x = rail_x, y = y + 36, width = rail_width, height = 20, children = {
    base.text({text = entry[3], font = ui.font, size = 12, weight = "medium",
      color = index == selected and C.on_surface or C.on_surface_variant, anchor = "center"})}})
  local destination = entry[1]
  base.hit_area({x = rail_x + 4, y = y - 4, width = rail_width - 8, height = 64,
    on_click = function() view(destination) end})
end
if H > 650 then
  ui:icon_button("power_settings_new", 32, H - 140, function() view("session") end, state.view == "session")
  base.stack({x = rail_x, y = H - 88, width = rail_width, height = 24, children = {
    base.text({text = datetime.format("%H:%M"), font = ui.font, size = 14, color = C.on_surface, anchor = "center"})}})
  base.stack({x = rail_x, y = H - 58, width = rail_width, height = 20, children = {
    base.text({text = datetime.format("%a %d"), font = ui.font, size = 12, color = C.on_surface_variant, anchor = "center"})}})
end

-- Desktop toolbar: launcher entry, tags, clock, audio, and theme.
local search_width = math.min(360, math.max(170, top_width * 0.32))
ui:rect(top_x + 8, top_y + 6, search_width, 44, C.container_high, 22, function() view("apps") end)
ui:icon("search", top_x + 14, top_y + 6, 40, 44)
ui:text("Search applications", top_x + 58, top_y + 18, "body_medium", C.on_surface_variant, search_width - 64)
local media_width = top_width >= 1050 and 300 or top_width >= 760 and 220 or 0
local tag_count = math.min(#tags, math.max(0, math.floor((top_width - search_width - media_width - 256) / 42)))
for index = 1, tag_count do
  local tag = tags[index]
  local x = top_x + search_width + 24 + (index - 1) * 42
  ui:rect(x, top_y + 12, 36, 32, tag.active and C.primary or C.container_high, 16, function()
    if native then zwwm.set_active_tags({tag.id})
    else hypr.request("/dispatch hl.dsp.focus({ workspace = '" .. tag.id .. "' })") end
  end)
  base.stack({x = x, y = top_y + 12, width = 36, height = 32, children = {
    base.text({text = tostring(tag.name), font = ui.font, size = 12,
      color = tag.active and C.on_primary or C.on_surface_variant, anchor = "center"})}})
end
if media_width > 0 then
  local media_x = W - 216 - media_width
  ui:rect(media_x, top_y + 8, media_width, 40, C.container_high, 20, function() view("media") end)
  ui:icon("music_note", media_x + 6, top_y + 8, 36, 40, C.primary)
  ui:text(player.available and player.title or "Nothing playing", media_x + 48, top_y + 10,
           "label_small", C.on_surface, media_width - 104)
  ui:text(player.available and player.artist or "Media controls", media_x + 48, top_y + 29,
           "body_small", C.on_surface_variant, media_width - 104)
  ui:icon_button(player.playback_status == "Playing" and "pause" or "play_arrow",
    media_x + media_width - 44, top_y + 8, function() mpris.play_pause() end)
end
ui:text(datetime.format("%H:%M"), W - 196, top_y + 18, "label", C.on_surface)
ui:icon_button(output and output.muted and "volume_off" or "volume_up", W - 118, top_y + 8,
  function() view("controls") end, state.view == "controls")
ui:icon_button(state.theme == "light" and "dark_mode" or "light_mode", W - 66, top_y + 8, function()
  state.theme = state.theme == "light" and "dark" or "light"; save()
end)

local function set_audio(id, node, value, dragging)
  if not node then return end
  local success = audio.set_volume(node.id, value)
  if success then state.drag = dragging and id or ""; state.drag_value = value; save() end
end
local function device_slider(id, node, x, y, width, symbol)
  local value = state.drag == id and state.drag_value or (node and node.volume or 0)
  ui:slider({id = id, x = x, y = y, width = width, value = value,
    icon = node and node.muted and "volume_off" or symbol, dragging = state.drag == id,
    enabled = node and node.can_control and node.volume_known or false,
    on_change = function(next_value, dragging) set_audio(id, node, next_value, dragging) end})
end
local function metric(label, value, subtitle, x, y, width)
  ui:rect(x, y, width, 112, C.container_high, 20)
  ui:text(label, x + 20, y + 14, "label_small", C.on_surface_variant, width - 40)
  ui:text(value, x + 20, y + 36, "headline", C.on_surface, width - 40)
  ui:text(subtitle, x + 20, y + 81, "body_small", C.on_surface_variant, width - 40)
end

if reveal > 0.001 then
  regions[#regions + 1] = {math.floor(px), py, math.ceil(pw), math.ceil(math.max(1, ph * reveal))}
  ui:scope(page_alpha * math.max(0, math.min(1, (reveal - 0.2) / 0.8)), {px, py, pw, math.max(1, ph * reveal)})
  local x, y, inner = px + 24, py + 24, pw - 48
  local titles = {home = "Your desktop", apps = "Applications", media = "Now playing",
                  controls = "Quick settings", alerts = "Notifications", session = "Session"}
  ui:text(titles[display_view] or "Your desktop", x, y, "title", C.on_surface, inner - 72)
  ui:icon_button("close", px + pw - 64, py + 16, close_panel)
  y = y + 56

  if display_view == "home" then
    ui:rect(x, y, inner, 120, C.primary_container, 24)
    ui:text(datetime.format("%H:%M"), x + 24, y + 12, "display", C.on_primary_container)
    ui:text(datetime.format("%A, %B %d"), x + 26, y + 78, "body", C.on_primary_container, inner - 52)
    local cell = (inner - 24) / 3
    metric("Memory", string.format("%.0f%%", memory * 100), string.format("%.1f GiB in use", (total - free) / 1048576), x, y + 136, cell)
    metric("System load", string.format("%.2f", load), "1-minute average", x + cell + 12, y + 136, cell)
    metric("Uptime", string.format("%dh", math.floor(uptime / 3600)), string.format("%dm since boot", math.floor(uptime / 60)), x + (cell + 12) * 2, y + 136, cell)
    ui:text("Make room for what matters", x, y + 270, "title_medium", C.on_surface)
    ui:text((os.getenv("USER") or "user") .. " · " .. (native and "zwwm" or is_hypr and "Hyprland" or "Wayland"), x, y + 300, "body_medium", C.on_surface_variant)
    ui:button("Browse apps", x, y + 340, 152, function() view("apps") end, "filled", "apps")
    ui:button("Terminal", x + 164, y + 340, 132, function() spawn(os.getenv("TERMINAL") or "ghostty") end, "tonal", "terminal")
    ui:button("Files", x + 308, y + 340, 108, function() spawn("xdg-open " .. quote(os.getenv("HOME") or "/")) end, "outlined", "folder")

  elseif display_view == "apps" then
    if reveal > 0.97 and state.view == "apps" then
      base.text_field({id = "material-app-query", x = x, y = y, width = inner, height = 56, radius = 28,
        font = ui.font, size = 16, placeholder = "Search apps", value = state.query, autofocus = true,
        color = {0, 0, 0, 0}, border_width = 0, text_color = C.on_surface,
        placeholder_color = C.on_surface_variant, clip = ui.clip,
        on_change = function(value) state.query = value; save() end,
        on_submit = function(query)
          local app = apps.search(query, 1)[1]
          if app then apps.launch(app.id); close_panel() end
        end})
      ui:rect(x, y, inner, 56, C.container_high, 28)
      local results = safe(apps.search, {}, state.query, 100)
      ui:text(#results .. " results", x + 8, y + 68, "label_small", C.on_surface_variant)
      base.listview({id = "material-apps-" .. state.query, x = x, y = y + 100,
        width = inner, height = math.max(1, ph - 204), item_height = 72, gap = 4, items = results,
        render = function(app)
          local icon
          if app.icon and app.icon ~= "" then
            local ok, asset = pcall(base.image, {icon = app.icon, width = 40, height = 40, x = 16, y = 16})
            if ok then icon = asset end
          end
          if not icon then icon = base.stack({x = 16, y = 16, width = 40, height = 40, children = {
            base.text({text = "apps", font = "Material Symbols Rounded", size = 28, color = C.primary, anchor = "center"})}}) end
          local name = base.text({text = app.name, x = 72, y = 12, font = ui.font, size = 16,
                                  color = C.on_surface, max_width = inner - 100, max_lines = 1})
          local subtitle = base.text({text = app.description ~= "" and app.description or app.id,
            x = 72, y = 40, font = ui.font, size = 12, color = C.on_surface_variant,
            max_width = inner - 100, max_lines = 1})
          local background = base.rectangle({width = inner, height = 72, radius = 16,
            color = C.container_high,
            on_click = function() apps.launch(app.id); close_panel() end,
            hover = {color = C.container_highest, duration = 0.15}})
          return base.stack({width = inner, height = 72, children = {background, icon, name, subtitle}})
        end})
    end

  elseif display_view == "media" then
    ui:rect(x, y, inner, 228, C.container_high, 24)
    ui:icon("music_note", x + 20, y + 20, 112, 112, C.primary, 56)
    if player.art_url and player.art_url:match("^file:///") then
      local image_path = player.art_url:sub(8):gsub("%%(%x%x)", function(code) return string.char(tonumber(code, 16)) end)
      pcall(base.image, {path = image_path, x = x + 20, y = y + 20, width = 112, height = 112, radius = 20, clip = ui.clip})
    end
    ui:text(player.available and player.title or "Nothing playing", x + 152, y + 22, "title", C.on_surface, inner - 180)
    ui:text(player.artist or "Open a media player to begin", x + 152, y + 62, "body", C.on_surface_variant, inner - 180)
    ui:text(player.album or "", x + 152, y + 94, "body_small", C.on_surface_variant, inner - 180)
    ui:icon_button("skip_previous", x + 20, y + 164, function() mpris.previous() end)
    ui:rect(x + 72, y + 156, 56, 56, C.primary, 28, function() mpris.play_pause() end)
    ui:icon(player.playback_status == "Playing" and "pause" or "play_arrow", x + 72, y + 156, 56, 56, C.on_primary, 28)
    ui:icon_button("skip_next", x + 140, y + 164, function() mpris.next() end)
    local length = player.length or 0
    ui:text(string.format("%d:%02d / %d:%02d", math.floor((player.position or 0) / 60),
      math.floor((player.position or 0) % 60), math.floor(length / 60), math.floor(length % 60)),
      x + 212, y + 172, "label", C.on_surface_variant)
    ui:text("Output volume", x, y + 256, "title_medium")
    device_slider("media-volume", output, x, y + 300, inner, "volume_up")

  elseif display_view == "controls" then
    ui:rect(x, y, inner, 224, C.container_high, 24)
    ui:text("Sound", x + 20, y + 16, "title_medium")
    ui:text(output and output.description or "No output device", x + 20, y + 46, "body_medium", C.on_surface_variant, inner - 100)
    ui:icon_button(output and output.muted and "volume_off" or "volume_up", x + inner - 60, y + 12,
      function() if output then audio.toggle_mute(output.id) end end, output and output.muted)
    device_slider("output", output, x + 20, y + 84, inner - 40, "volume_up")
    ui:text(input and input.description or "No microphone", x + 20, y + 140, "body_medium", C.on_surface_variant, inner - 100)
    ui:icon_button(input and input.muted and "mic_off" or "mic", x + inner - 60, y + 130,
      function() if input then audio.toggle_mute(input.id) end end, input and input.muted)
    device_slider("input", input, x + 20, y + 174, inner - 40, "mic")
    ui:text("Appearance", x, y + 250, "title_medium")
    ui:text("Dark theme", x, y + 289, "body")
    ui:switch("theme", state.theme ~= "light", x + inner - 52, y + 279, function()
      state.theme = state.theme == "light" and "dark" or "light"; save()
    end)
    ui:button("Wi-Fi settings", x, y + 338, 160, function() spawn("nm-connection-editor") end, "tonal", "wifi")
    ui:button("Bluetooth", x + 172, y + 338, 148, function() spawn("blueman-manager") end, "tonal", "bluetooth")
    ui:button("Sound settings", x + 332, y + 338, 172, function() spawn("pavucontrol") end, "outlined", "tune")

  elseif display_view == "alerts" then
    if not own_notifications then
      ui:icon("notifications_none", x, y + 20, inner, 96, C.primary, 64)
      ui:text("Notification service is not owned by this shell", x + 8, y + 142, "title_medium", C.on_surface, inner - 16)
      ui:text(notification_error ~= "" and notification_error or "Enable VW_MATERIAL_NOTIFICATIONS=1 when no other daemon owns the bus name.",
               x + 8, y + 180, "body_medium", C.on_surface_variant, inner - 16, 3)
    else
      ui:text("Do not disturb", x, y + 8, "body")
      ui:switch("dnd", notifications.dnd(), x + inner - 52, y, function() notifications.set_dnd(not notifications.dnd()) end)
      ui:button("Clear history", x, y + 56, 160, function() notifications.clear() end, "outlined", "delete")
      if reveal > 0.97 then
        base.listview({id = "material-alerts", x = x, y = y + 116, width = inner,
          height = math.max(1, ph - 220), item_height = 112, gap = 8, items = alerts,
          render = function(item)
            local background = base.rectangle({width = inner, height = 112, radius = 20,
              color = C.container_high})
            local name = base.text({text = item.app_name, x = 16, y = 12, font = ui.font, size = 12, color = C.on_surface_variant,
                                    max_width = inner - 32, max_lines = 1})
            local title = base.text({text = item.summary, x = 16, y = 37, font = ui.font, size = 16, color = C.on_surface,
                                     max_width = inner - 32, max_lines = 1})
            local body = base.text({text = item.body, x = 16, y = 68, font = ui.font, size = 12, color = C.on_surface_variant,
                                    max_width = inner - 32, max_lines = 2})
            return base.stack({width = inner, height = 112, children = {background, name, title, body}})
          end})
      end
    end

  elseif display_view == "session" then
    ui:text("Session actions", x, y, "headline")
    ui:text("Choose an action for your current desktop session.", x, y + 46, "body_medium", C.on_surface_variant)
    ui:button("Lock", x, y + 98, 140, function() spawn("swaylock") end, "filled", "lock")
    ui:button("Suspend", x + 152, y + 98, 152, function() spawn("systemctl suspend") end, "tonal", "bedtime")
    ui:button("Reboot", x, y + 154, 140, function() spawn("systemctl reboot") end, "outlined", "restart_alt")
    ui:button("Power off", x + 152, y + 154, 152, function() spawn("systemctl poweroff") end, "outlined", "power_settings_new")
    ui:button("Exit this shell", x, y + 250, 188, function() window.close() end, "tonal", "close")
  end
  ui:scope()
end

surface.set({width = W, height = H, anchor = "top_left", layer = os.getenv("VW_MATERIAL_LAYER") or "top",
  namespace = "vw:material-shell", input_regions = regions,
  focus_grab = state.view ~= "",
  on_dismiss = function(event)
    if event.reason ~= "released" then close_panel() end
  end})
