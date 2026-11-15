local base = require("vw.base")
local datetime = require("vw.datetime")
local poll = require("vw.poll")
local socket = require("vw.socket")
local applications = require("vw.services.applications")
local hyprland = require("vw.services.hyprland")

local colors = {
  panel = { 0.035, 0.045, 0.065, 0.96 },
  panel_alt = { 0.065, 0.08, 0.115, 0.96 },
  border = { 0.22, 0.28, 0.38, 0.85 },
  text = { 0.91, 0.94, 1.0, 1.0 },
  muted = { 0.56, 0.62, 0.72, 1.0 },
  red = { 0.95, 0.12, 0.2, 1.0 },
  red_hot = { 1.0, 0.28, 0.36, 1.0 },
  cyan = { 0.14, 0.78, 0.88, 1.0 },
  green = { 0.25, 0.86, 0.55, 1.0 },
}

local function safe(call, fallback)
  if not hyprland.available() then return fallback end
  local ok, value = pcall(call)
  return ok and value or fallback
end

local function dispatch(expression)
  local ok = pcall(hyprland.request, "/dispatch " .. expression)
  return ok
end

local function label(text, size, color, weight, options)
  local properties = {
    text = text,
    font = "JetBrains Mono NF",
    size = size or 14,
    weight = weight or "medium",
    color = color or colors.text,
    anchor = "center",
  }
  for key, value in pairs(options or {}) do properties[key] = value end
  return base.text(properties)
end

local function pill(options)
  local background = base.rectangle({
    width = options.width,
    height = options.height or 38,
    radius = options.radius or 11,
    color = options.color or colors.panel_alt,
    color2 = options.color2 or options.color or colors.panel,
    gradient = options.gradient or "vertical",
    border_width = options.border_width or 1,
    border_color = options.border_color or colors.border,
    on_click = options.on_click,
    hover = {
      color = options.hover_color or { 0.12, 0.16, 0.23, 1.0 },
      scale = options.hover_scale or 1.025,
      duration = options.duration or 0.14,
    },
  })
  return base.stack({
    width = options.width,
    height = options.height or 38,
    children = { background, options.content },
  })
end

local workspaces = safe(hyprland.workspaces, {})
table.sort(workspaces, function(left, right) return left.id < right.id end)
if #workspaces == 0 then workspaces = { { id = 1, name = "1" } } end

local active_workspace = safe(hyprland.active_workspace, { id = -1 })
local active_window = safe(hyprland.active_window, { title = "Desktop", class = "desktop" })
local window_title = active_window.title or "Desktop"
local window_class = active_window.class or active_window.initialClass or "desktop"

local load = poll.command("awk '{ printf \"LOAD %.2f\", $1 }' /proc/loadavg", 3)
local memory = poll.command(
  "awk '/MemTotal/ { t=$2 } /MemAvailable/ { a=$2 } END { printf \"MEM %.0f%%\", (t-a)*100/t }' /proc/meminfo",
  5
)
local signature = os.getenv("HYPRLAND_INSTANCE_SIGNATURE") or ""
local runtime = os.getenv("XDG_RUNTIME_DIR") or "/run/user/1000"
local event_socket = runtime .. "/hypr/" .. signature .. "/.socket2.sock"
local ipc_online = signature ~= "" and socket.available(event_socket)
local terminal = applications.search("ghostty", 1)[1]

local brand_content = base.row({
  width = 164,
  height = 38,
  padding = 4,
  gap = 9,
  children = {
    base.image({
      path = "/home/blx/.bprofile/mcpfp - AR3608.jpg",
      width = 30,
      height = 30,
      radius = 15,
      anchor = "center",
    }),
    base.column({
      width = 113,
      height = 30,
      gap = 0,
      children = {
        label("VW SHELL", 13, colors.text, "bold"),
        label("VULKAN / LUA", 9, colors.muted, "medium"),
      },
    }),
  },
})

local brand = pill({
  width = 164,
  content = brand_content,
  color = { 0.22, 0.035, 0.065, 1.0 },
  color2 = { 0.07, 0.055, 0.1, 1.0 },
  border_color = { 0.78, 0.12, 0.22, 0.9 },
  hover_color = { 0.5, 0.055, 0.1, 1.0 },
  on_click = function(event)
    if event.button == 272 then
      if terminal then
        pcall(applications.launch, terminal.id)
      else
        dispatch("hl.dsp.exec_cmd('ghostty')")
      end
    end
  end,
})

local workspace_strip = base.row({
  height = 38,
  padding = 0,
  gap = 6,
  children = {
    base.repeater({
      items = workspaces,
      render = function(workspace)
        local active = workspace.id == active_workspace.id
        return pill({
          width = 38,
          height = 38,
          radius = 12,
          content = label(tostring(workspace.name), 13,
                          active and { 1, 1, 1, 1 } or colors.muted,
                          active and "bold" or "medium"),
          color = active and colors.red or colors.panel_alt,
          color2 = active and { 0.68, 0.045, 0.1, 1.0 } or colors.panel,
          border_color = active and { 1.0, 0.32, 0.4, 0.9 } or colors.border,
          hover_color = active and colors.red_hot or { 0.14, 0.19, 0.27, 1.0 },
          hover_scale = 1.06,
          on_click = function(event)
            if event.button == 272 then
              dispatch("hl.dsp.focus({ workspace = '" .. workspace.id .. "' })")
            end
          end,
        })
      end,
    }),
  },
})

local window = pill({
  width = 420,
  content = base.column({
    width = 404,
    height = 31,
    gap = 0,
    children = {
      label(window_title, 12, colors.text, "semibold",
            { max_width = 390, max_lines = 1, align = "center", ellipsize = "end" }),
      label(string.upper(window_class), 9, colors.muted, "medium",
            { max_width = 390, max_lines = 1, align = "center", ellipsize = "middle" }),
    },
  }),
  hover_color = { 0.085, 0.115, 0.17, 1.0 },
})

local special = pill({
  width = 42,
  content = label("SP", 11, colors.cyan, "bold"),
  hover_color = { 0.05, 0.32, 0.38, 1.0 },
  border_color = { 0.1, 0.45, 0.52, 0.9 },
  on_click = function(event)
    if event.button == 272 then dispatch("hl.dsp.workspace.toggle_special('magic')") end
  end,
})

local app_search = base.text_field({
  id = "application-search",
  width = 176,
  height = 38,
  radius = 11,
  size = 12,
  font = "JetBrains Mono NF",
  placeholder = "Search apps...",
  color = colors.panel,
  border_color = colors.border,
  max_length = 48,
  on_submit = function(query)
    local match = applications.search(query, 1)[1]
    if match then pcall(applications.launch, match.id) end
  end,
})

local load_pill = pill({ width = 92, content = label(load, 11, colors.cyan, "bold") })
local memory_pill = pill({ width = 92, content = label(memory, 11, colors.green, "bold") })
local ipc_pill = base.lazy({
  visible = true,
  render = function()
    return pill({
      width = 58,
      content = base.column({
        width = 48,
        height = 28,
        gap = 0,
        children = {
          label(ipc_online and "IPC" or "OFF", 10, ipc_online and colors.green or colors.red, "bold"),
          label(ipc_online and "LIVE" or "DOWN", 8, colors.muted, "medium"),
        },
      }),
      border_color = ipc_online and { 0.12, 0.42, 0.3, 0.9 } or { 0.58, 0.08, 0.13, 0.9 },
    })
  end,
})

local clock = pill({
  width = 146,
  content = base.column({
    width = 132,
    height = 31,
    gap = 0,
    children = {
      label(datetime.format("%H:%M:%S"), 14, colors.text, "bold"),
      label(string.upper(datetime.format("%a %d %b")), 9, colors.muted, "medium"),
    },
  }),
  color = { 0.08, 0.095, 0.14, 1.0 },
  color2 = { 0.045, 0.055, 0.085, 1.0 },
  border_color = { 0.3, 0.35, 0.5, 0.85 },
  hover_color = { 0.13, 0.16, 0.25, 1.0 },
})

base.row({
  anchor = "bottom",
  layer = "top",
  exclusive = true,
  margin = { 0, 0, 0, 0 },
  padding = 10,
  gap = 8,
  radius = 16,
  children = {
    brand,
    workspace_strip,
    special,
    app_search,
    window,
    load_pill,
    memory_pill,
    ipc_pill,
    clock,
  },
})
