-- BlxShell-inspired single-surface shell for vw.
local base = require("vw.base")
local surface = require("vw.surface")
local datetime = require("vw.datetime")
local pipewire = require("vw.services.pipewire")
local apps = require("vw.services.applications")
local mpris = require("vw.services.mpris")
local zwwm = require("vw.services.zwwm")
local hypr = require("vw.services.hyprland")
local animation = require("vw.animation")

local function motion(id, target, duration, easing, initial, constraints)
  local options = {duration = duration or 0.3, easing = easing or "out_cubic", initial = initial,
                   mass = 1, stiffness = 240, damping = 19}
  if easing == "bezier" then options.bezier = {0.2, 0.8, 0.25, 1} end
  if constraints then options.min, options.max = constraints.min, constraints.max end
  return animation.value("blxshell-" .. id, target,
                         options)
end

local function safe(fn, fallback, ...)
  local ok, value = pcall(fn, ...)
  return ok and value or fallback
end
local function read(path)
  local file = io.open(path, "r")
  if not file then return "" end
  local value = file:read("*a") or ""
  file:close()
  return value
end
local function quote(value) return "'" .. tostring(value):gsub("'", "'\\''") .. "'" end
local function spawn(command) os.execute(command .. " >/dev/null 2>&1 &") end
local runtime = os.getenv("XDG_RUNTIME_DIR") or "/tmp"
local state_path = os.getenv("VW_SHELL_STATE") or (runtime .. "/vw-blxshell-state")
local saved = read(state_path)
local state = {view = saved:match("^([^\n]*)") or "",
               tab = saved:match("^[^\n]*\n([^\n]*)") or "overview",
               query = saved:match("^[^\n]*\n[^\n]*\n([^\n]*)") or "",
               side = saved:match("^[^\n]*\n[^\n]*\n[^\n]*\n([^\n]*)") or "system",
               reveal = tonumber(saved:match("^[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n([^\n]*)")),
               dragging = saved:match("^[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n[^\n]*\n([^\n]*)") == "1"}
local function save()
  local file = io.open(state_path .. ".tmp", "w")
  if not file then return end
  file:write(state.view, "\n", state.tab, "\n", state.query:gsub("[\r\n]", " "),
             "\n", state.side, "\n", tostring(state.reveal or 0), "\n",
             state.dragging and "1" or "0", "\n")
  file:close()
  os.rename(state_path .. ".tmp", state_path)
end
local function toggle(view)
  if view == "system" or view == "power" then state.side = view end
  state.dragging = false
  state.view = state.view == view and "" or view
  save()
end

local C = {
  surface = {0.063, 0.078, 0.098, 1}, card = {0.106, 0.153, 0.196, 1},
  high = {0.165, 0.224, 0.278, 1}, accent = {0.612, 0.792, 1, 1},
  accent_text = {0.063, 0.125, 0.18, 1}, text = {0.918, 0.945, 0.965, 1},
  muted = {0.576, 0.663, 0.725, 1}, green = {0.616, 0.886, 0.753, 1},
  red = {1, 0.561, 0.58, 1}, clear = {0, 0, 0, 0},
}
local FONT = os.getenv("VW_SHELL_FONT") or "Rubik"
local ICON = "Material Symbols Rounded"
local MS = {apps = "apps", dashboard = "home", media = "music_note", system = "settings", close = "close",
            previous = "skip_previous", play = "play_arrow", pause = "pause", next = "skip_next", power = "power_settings_new",
            volume = "volume_up", terminal = "terminal", files = "folder", shot = "photo_camera", lock = "lock"}
local native = safe(zwwm.available, false)
local is_hypr = not native and safe(hypr.available, false)
local monitor
if native then monitor = safe(zwwm.outputs, {})[1]
elseif is_hypr then
  for _, candidate in ipairs(safe(hypr.monitors, {})) do
    if not monitor or candidate.focused then monitor = candidate end
  end
end
monitor = monitor or {}
local W = tonumber(os.getenv("VW_SHELL_WIDTH")) or monitor.logical_width or
          math.floor((monitor.width or 1920) / (monitor.scale or 1))
local H = tonumber(os.getenv("VW_SHELL_HEIGHT")) or monitor.logical_height or
          math.floor((monitor.height or 1080) / (monitor.scale or 1))
local rail, frame = 55, 12
local windows = native and safe(zwwm.toplevels, {}) or safe(hypr.clients, {})
local active = {}
for _, window in ipairs(windows) do
  if native and ((window.state or 0) & 4) ~= 0 then active = window end
end
if is_hypr then active = safe(hypr.active_window, {}) end
local tags = native and safe(zwwm.tags, {}) or {}
if is_hypr then
  local current = safe(hypr.active_workspace, {id = 1}).id
  for id = 1, 6 do tags[id] = {id = id, name = tostring(id), active = id == current} end
end
local player = safe(mpris.state, {available = false, playback_status = "Stopped"})
local keyboard = native and safe(zwwm.keyboard, {})[1] or nil
local memory_text = read("/proc/meminfo")
local total = tonumber(memory_text:match("MemTotal:%s+(%d+)")) or 1
local available = tonumber(memory_text:match("MemAvailable:%s+(%d+)")) or total
local memory = (total - available) / total
local load = tonumber(read("/proc/loadavg"):match("^([%d.]+)")) or 0
local uptime_seconds = tonumber(read("/proc/uptime"):match("^([%d.]+)")) or 0
local uptime_text = string.format("Up %dh %02dm", math.floor(uptime_seconds / 3600),
                                 math.floor(uptime_seconds / 60) % 60)
local audio = safe(pipewire.default_sink, nil)
local volume = audio and audio.volume or 0
local function change_volume(delta)
  local node = pipewire.default_sink()
  if node then pipewire.set_volume(node.id, math.max(0, math.min(1, node.volume + delta))) end
end
local dashboard = motion("dashboard", state.view == "dashboard" and 1 or 0, 0.36, "spring", 0,
                         {min = 0, max = 1})
local launcher = motion("launcher", state.view == "apps" and 1 or 0, 0.32, "bezier", 0,
                        {min = 0, max = 1})
local sidebar_target = state.dragging and (state.reveal or 0) or
    ((state.view == "system" or state.view == "power") and 1 or 0)
local sidebar = motion("sidebar", sidebar_target, state.dragging and 0 or 0.36, "spring", 0,
                       {min = 0, max = 1})
local content_alpha, content_clip = 1, nil
local function faded(color)
  return {color[1], color[2], color[3], color[4] * content_alpha}
end

local function text(value, x, y, size, color, width, font)
  return base.text({text = tostring(value or ""), x = x, y = y, size = size or 14,
    font = font or FONT, color = faded(color or C.text), max_width = width or 0,
    max_lines = 1, ellipsize = "end", anchor = "top_left", clip = content_clip})
end
local function rect(x, y, w, h, color, radius, callback)
  return base.rectangle({x = x, y = y, width = w, height = h, color = faded(color or C.card),
    radius = radius or 18, on_click = callback, clip = content_clip,
    hover = callback and {color = faded(C.high), duration = 0.15} or nil})
end
local function button(label, x, y, w, h, callback, selected, icon)
  rect(x, y, w, h, selected and C.accent or C.card, h / 2, callback)
  text(label, x + (icon and (w - 18) / 2 or 14), y + (h - (icon and 18 or 13)) / 2 - 2,
       icon and 18 or 13, selected and C.accent_text or C.text, w - 16,
       icon and ICON or FONT)
end
local function media_controls(x, y)
  button(MS.previous, x, y, 44, 36, function() mpris.previous() end, false, true)
  button(player.playback_status == "Playing" and MS.pause or MS.play, x + 52, y,
         44, 36, function() mpris.play_pause() end, true, true)
  button(MS.next, x + 104, y, 44, 36, function() mpris.next() end, false, true)
end
local function card(title, value, x, y, w, h, fraction)
  rect(x, y, w, h)
  text(title, x + 16, y + 13, 11, C.muted)
  text(value, x + 16, y + 35, 25, C.accent, w - 32)
  if fraction then
    rect(x + 16, y + h - 20, w - 32, 5, C.high, 2)
    rect(x + 16, y + h - 20, math.max(1, (w - 32) * math.min(1, fraction)), 5, C.accent, 2)
  end
end

local panels = {}
local regions = {{0, 0, rail, H}, {0, 0, W, frame}, {W - frame, 0, frame, H},
                 {0, H - frame, W, frame}, {W - 64, 0, 64, 64}}
local function region(x, y, w, h)
  regions[#regions + 1] = {math.max(0, math.floor(x)), math.max(0, math.floor(y)),
                          math.ceil(w), math.ceil(h)}
end
local function member(x, y, w, h, radius)
  panels[#panels + 1] = {x = x, y = y, width = w, height = h, radius = radius or 25}
  region(x, y, w, h)
end
local panel_w = math.min(850, W - rail - 56)
local panel_x = math.floor((W - panel_w) / 2)
local panel_h = math.min(410, H - 64)
local app_w = math.min(650, W - rail - 56)
local app_x = math.floor((W - app_w) / 2)
local app_h = math.min(480, H - 80)
local app_y = H - frame - app_h * launcher + 4
local side_w = math.min(360, W - rail - 32)
local side_x = W - frame - side_w * sidebar + 4
if dashboard > 0.001 then member(panel_x, frame - 4, panel_w, math.max(1, panel_h * dashboard)) end
if dashboard < 0.999 then member(math.floor((W - 320) / 2), frame - 4, 320, 48, 24) end
if launcher > 0.001 then member(app_x, app_y, app_w, math.max(1, app_h * launcher), 30) end
if sidebar > 0.001 then
  member(side_x, frame - 4, math.max(1, side_w * sidebar), math.min(H - 20, 520), 24)
end
-- Screen frame and rail, then each open panel as a rounded surface.
for _, edge in ipairs({{0, 0, rail, H}, {0, 0, W, frame}, {W - frame, 0, frame, H}, {0, H - frame, W, frame}}) do
  base.rectangle({x = edge[1], y = edge[2], width = edge[3], height = edge[4], color = C.surface})
end
for _, panel in ipairs(panels) do
  base.rectangle({x = panel.x, y = panel.y, width = panel.width, height = panel.height,
    radius = panel.radius, color = C.surface})
end

-- Left rail: launcher, tags, active app, sound, keyboard, clock, power.
button(MS.apps, 7, 17, 41, 36, function() toggle("apps") end, state.view == "apps", true)
local tag_y = 65
local active_tag_index = 1
for index, tag in ipairs(tags) do if tag.active then active_tag_index = index end end
local indicator_y = motion("workspace", 65 + (active_tag_index - 1) * 40, 0.3, "spring", nil,
                           {min = 0, max = H - 34})
rect(9, indicator_y, 37, 34, C.accent, 17)
for index, tag in ipairs(tags) do
  if index > 8 then break end
  local id = tag.id
  rect(9, tag_y, 37, 34, C.clear, 17, function()
    if native then zwwm.set_active_tags({id})
    else hypr.request("/dispatch hl.dsp.focus({ workspace = '" .. id .. "' })") end
  end)
  text(tag.active and "●" or tostring(tag.name), 22, tag_y + 7, 13,
       tag.active and C.accent_text or C.muted, 26)
  tag_y = tag_y + 40
end
button(MS.dashboard, 7, math.max(tag_y + 10, math.floor(H * 0.47)), 41, 36,
       function() toggle("dashboard") end, state.view == "dashboard", true)
text((active.app_id or active.class or "desktop"):sub(1, 3):upper(), 12,
     math.max(tag_y + 54, math.floor(H * 0.47) + 48), 10, C.muted, 35)
button(MS.volume, 7, H - 226, 41, 36, function() toggle("system") end, false, true)
text((keyboard and keyboard.layout or "EN"):sub(1, 3):upper(), 16, H - 174, 10, C.muted, 32)
text(datetime.format("%H"), 15, H - 143, 21, C.accent)
text(datetime.format("%M"), 15, H - 116, 21, C.accent)
button(MS.system, 7, H - 78, 41, 30, function() toggle("system") end,
       state.view == "system", true)
button(MS.power, 7, H - 42, 41, 30, function() toggle("power") end,
       state.view == "power", true)

if dashboard < 0.999 then
  content_alpha = 1 - dashboard
  local island_x = math.floor((W - 320) / 2)
  base.hit_area({x = island_x, y = frame - 4, width = 320, height = 48,
    on_click = function() toggle("dashboard") end})
  text(player.available and MS.media or MS.dashboard, island_x + 18, frame + 10, 17, C.accent, 24, ICON)
  text(player.available and player.title or datetime.format("%A, %d %B"),
       island_x + 52, frame + 3, 13, C.text, 238)
  text(player.available and player.artist or "Dashboard", island_x + 52, frame + 23, 10, C.muted, 238)
  content_alpha = 1
end

if dashboard > 0.001 then
  content_alpha = math.max(0, math.min(1, (dashboard - 0.2) / 0.8))
  content_clip = {panel_x, frame - 4, panel_w, math.max(1, panel_h * dashboard)}
  local x, y = panel_x + 20, frame + 10
  local tabs = {"overview", "performance", "media"}
  local tab_index = state.tab == "performance" and 1 or state.tab == "media" and 2 or 0
  local tab_x = motion("tab", x + tab_index * 132, 0.22, "spring", nil,
                      {min = panel_x, max = panel_x + panel_w - 122})
  rect(tab_x, y, 122, 34, C.accent, 17)
  for index, tab in ipairs(tabs) do
    local tx = x + (index - 1) * 132
    rect(tx, y, 122, 34, C.clear, 17, function() state.tab = tab; save() end)
    text(tab:sub(1, 1):upper() .. tab:sub(2), tx + 14, y + 8, 13,
         state.tab == tab and C.accent_text or C.text)
  end
  button(MS.close, panel_x + panel_w - 60, y, 38, 34, function() toggle("dashboard") end, false, true)
  y = y + 52
  if state.tab == "performance" then
    local w = (panel_w - 52) / 2
    card("MEMORY", string.format("%.0f%%", memory * 100), x, y, w, 120, memory)
    card("SYSTEM LOAD", string.format("%.2f", load), x + w + 12, y, w, 120, math.min(load / 8, 1))
    text("Linux · " .. (os.getenv("USER") or "user"), x + 16, y + 156, 18, C.text)
    text("Metrics read directly from /proc", x + 16, y + 189, 13, C.muted)
    text(uptime_text, x + 16, y + 222, 14, C.accent)
  elseif state.tab == "media" then
    text(player.available and player.title or "Nothing is playing", x + 16, y + 16, 25, C.text, panel_w - 80)
    text(player.artist or "Open a media player to begin", x + 16, y + 58, 16, C.muted, panel_w - 80)
    text(player.album or "", x + 16, y + 89, 13, C.muted, panel_w - 80)
    media_controls(x + 16, y + 132)
    local length = player.length or 0
    card("POSITION", string.format("%d:%02d / %d:%02d", math.floor((player.position or 0) / 60),
      math.floor((player.position or 0) % 60), math.floor(length / 60), math.floor(length % 60)),
      x + 210, y + 120, math.max(160, panel_w - 250), 105,
      length > 0 and (player.position or 0) / length or 0)
  else
    local left_w = math.floor((panel_w - 52) * 0.61)
    local right_x, right_w = x + left_w + 12, panel_w - 52 - left_w
    rect(x, y, left_w, 92)
    text("Hello, " .. (os.getenv("USER") or "user"), x + 18, y + 14, 25, C.text)
    text(datetime.format("%A, %d %B %Y"), x + 18, y + 54, 14, C.muted)
    card("MEMORY", string.format("%.0f%%", memory * 100), right_x, y, right_w, 92, memory)
    rect(x, y + 104, left_w, 212)
    text(datetime.format("%B %Y"), x + 18, y + 120, 18, C.accent)
    local now = os.date("*t")
    local first = os.date("*t", os.time({year = now.year, month = now.month, day = 1, hour = 12})).wday
    local days = os.date("*t", os.time({year = now.year, month = now.month + 1, day = 0, hour = 12})).day
    local cell = (left_w - 28) / 7
    for index, name in ipairs({"S", "M", "T", "W", "T", "F", "S"}) do
      text(name, x + 17 + (index - 1) * cell, y + 157, 11, C.muted)
    end
    for day = 1, days do
      local slot = first - 1 + day - 1
      local cx, cy = x + 12 + (slot % 7) * cell, y + 179 + math.floor(slot / 7) * 23
      if day == now.day then rect(cx - 3, cy - 2, 27, 23, C.accent, 11) end
      text(tostring(day), cx + 2, cy, 12, day == now.day and C.accent_text or C.text)
    end
    rect(right_x, y + 104, right_w, 212)
    text("Now playing", right_x + 16, y + 120, 12, C.muted)
    text(player.title or "No media", right_x + 16, y + 154, 16, C.text, right_w - 32)
    text(player.artist or "", right_x + 16, y + 187, 12, C.muted, right_w - 32)
    media_controls(right_x + 14, y + 236)
  end
  content_alpha, content_clip = 1, nil
end

if launcher > 0.001 then
  content_alpha = math.max(0, math.min(1, (launcher - 0.2) / 0.8))
  content_clip = {app_x, app_y, app_w, math.max(1, app_h * launcher)}
  text("Applications", app_x + 22, app_y + 20, 22, C.text)
  button(MS.close, app_x + app_w - 60, app_y + 16, 38, 34, function() toggle("apps") end, false, true)
  if launcher > 0.98 then
  base.text_field({id = "blxshell-launcher", x = app_x + 20, y = app_y + 65,
    width = app_w - 40, height = 42, radius = 21, placeholder = "Search applications…",
    font = FONT, size = 14, clip = content_clip, autofocus = true,
    on_change = function(value) state.query = value; save() end,
    on_submit = function(query)
      local app = apps.search(query, 1)[1]
      if app then apps.launch(app.id); state.view = ""; save() end
    end})
  local results = safe(apps.search, {}, state.query, 100)
  base.listview({id = "blxshell-apps-" .. state.query, x = app_x + 20, y = app_y + 120,
    width = app_w - 40, height = app_h - 154, item_height = 54, gap = 6, radius = 16,
    items = results, render = function(app)
      return base.anchor_layout({width = app_w - 40, height = 54, children = {
        {anchors = {left = true, right = true, top = true, bottom = true},
         render = function(bounds)
           return base.rectangle({width = bounds.width, height = bounds.height, radius = 16,
             color = C.card, on_click = function() apps.launch(app.id); state.view = ""; save() end,
             hover = {color = C.high}})
         end},
        {anchors = {left = true, top = true}, margin = {10, 0, 0, 12},
         width = 34, height = 34, render = function(bounds)
           if app.icon and app.icon ~= "" then
             local ok, image = pcall(base.image, {icon = app.icon, width = bounds.width, height = bounds.height})
             if ok then return image end
           end
           return base.text({text = MS.apps, font = ICON, size = 24, color = C.accent})
         end},
        {anchors = {left = true, right = true, top = true}, margin = {8, 16, 0, 58},
         height = 20, render = function(bounds)
           return base.text({text = app.name, font = FONT, size = 14,
             max_width = bounds.width, max_lines = 1, color = C.text})
         end},
        {anchors = {left = true, right = true, top = true}, margin = {31, 16, 0, 58},
         height = 15, render = function(bounds)
           return base.text({text = app.description or app.id, font = FONT, size = 10,
             max_width = bounds.width, max_lines = 1, color = C.muted})
         end},
      }})
    end})
  text("Enter to launch · scroll to browse", app_x + 22, H - frame - 24, 11, C.muted)
  end
  content_alpha, content_clip = 1, nil
end

if sidebar > 0.001 then
  content_alpha = sidebar
  content_clip = {side_x, frame - 4, math.max(1, side_w * sidebar), math.min(H - 20, 520)}
  local start_reveal = sidebar
  base.drag({x = side_x, y = frame - 4, width = side_w, height = math.min(H - 20, 520),
    clip = content_clip,
    on_drag_start = function() state.dragging = true; state.reveal = start_reveal; save() end,
    on_drag = function(event)
      state.reveal = math.max(0, math.min(1, start_reveal - event.dx / 180))
      save()
    end,
    on_drag_end = function(event)
      local reveal = math.max(0, math.min(1, start_reveal - event.dx / 180))
      state.dragging = false
      state.view = not event.cancelled and reveal <= 0.65 and "" or state.side
      save()
    end})
  local x, y = side_x + 20, frame + 12
  text(state.side == "power" and "Session" or "Quick controls", x, y, 23, C.text)
  button(MS.close, side_x + side_w - 58, y - 2, 38, 34,
         function() state.view = ""; save() end, false, true)
  y = y + 62
  if state.side == "system" then
    card("VOLUME", not audio and "No device" or audio.muted and "Muted" or string.format("%.0f%%", volume * 100),
      x, y, side_w - 40, 95, math.min(volume, 1))
    button("−", x, y + 106, 60, 36, function() change_volume(-0.05) end)
    button("Mute", x + 70, y + 106, 100, 36, function()
      local node = pipewire.default_sink()
      if node then pipewire.toggle_mute(node.id) end
    end)
    button("+", x + 180, y + 106, 60, 36, function() change_volume(0.05) end)
    local actions = {{"Terminal", os.getenv("TERMINAL") or "ghostty"},
                     {"Files", "xdg-open " .. quote(os.getenv("HOME") or "/")},
                     {"Screenshot", "grim " .. quote((os.getenv("HOME") or "/tmp") .. "/Pictures/vw-" .. os.time() .. ".png")},
                     {"Sound settings", "pavucontrol"}}
    for index, action in ipairs(actions) do
      button(action[1], x, y + 160 + (index - 1) * 47, side_w - 40, 39, function() spawn(action[2]) end)
    end
  else
    local actions = {{"Lock screen", "swaylock"}, {"Suspend", "systemctl suspend"},
                     {"Log out", native and "zwwmctl dispatch exit" or "hyprctl dispatch exit"},
                     {"Reboot", "systemctl reboot"}, {"Power off", "systemctl poweroff"}}
    for index, action in ipairs(actions) do
      button(action[1], x, y + (index - 1) * 54, side_w - 40, 42, function() spawn(action[2]) end)
    end
  end
  content_alpha, content_clip = 1, nil
end

-- The original top-right tug pulls the sidebar inward; the gesture stays
-- captured even after leaving its initial 64px target.
if sidebar < 0.95 then base.drag({x = W - 64, y = 0, width = 64, height = 64,
  on_drag_start = function()
    state.side = "system"; state.view = "system"
    state.dragging = true; state.reveal = 0; save()
  end,
  on_drag = function(event)
    state.reveal = math.max(0, math.min(1, math.max(-event.dx, event.dy) / 120))
    save()
  end,
  on_drag_end = function(event)
    local reveal = math.max(0, math.min(1, math.max(-event.dx, event.dy) / 120))
    state.dragging = false
    state.view = not event.cancelled and reveal >= 0.35 and "system" or ""; save()
  end}) end

-- Final surface metadata: one fullscreen layer surface; center stays click-through.
surface.set({width = W, height = H, anchor = "top_left",
  layer = os.getenv("VW_SHELL_LAYER") or "bottom", namespace = "vw:blxshell",
  input_regions = regions, focus_grab = state.view ~= "",
  on_dismiss = function(event)
    if event.reason ~= "released" then
      state.view = ""; state.dragging = false; save()
    end
  end})
