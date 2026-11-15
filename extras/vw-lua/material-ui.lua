-- Material 3 tokens adapted from Google's Material Web token sources.
local base = require("vw.base")
local animation = require("vw.animation")
local M = {}
local function hex(value)
  return {tonumber(value:sub(1, 2), 16) / 255, tonumber(value:sub(3, 4), 16) / 255,
          tonumber(value:sub(5, 6), 16) / 255, 1}
end
local palettes = {
  dark = {surface = hex("131314"), container_low = hex("1a1b1e"), container = hex("1e1f20"),
    container_high = hex("282a2c"), container_highest = hex("333537"),
    on_surface = hex("e3e3e3"), on_surface_variant = hex("c4c7c5"),
    primary = hex("a8c7fa"), on_primary = hex("062e6f"), primary_container = hex("004a77"),
    on_primary_container = hex("c2e7ff"), secondary_container = hex("33485a"),
    on_secondary_container = hex("d7e3f5"), tertiary = hex("a8dab5"),
    outline = hex("8e918f"), outline_variant = hex("444746"), error = hex("ffb4ab")},
  light = {surface = hex("f8fafd"), container_low = hex("f0f4f9"), container = hex("edf2fa"),
    container_high = hex("e6edf7"), container_highest = hex("dfe6ef"),
    on_surface = hex("1f1f1f"), on_surface_variant = hex("444746"),
    primary = hex("0b57d0"), on_primary = hex("ffffff"), primary_container = hex("c2e7ff"),
    on_primary_container = hex("001d35"), secondary_container = hex("d7e3f5"),
    on_secondary_container = hex("101c2b"), tertiary = hex("386a45"),
    outline = hex("747775"), outline_variant = hex("c4c7c5"), error = hex("ba1a1a")},
}
local type_scale = {
  display = {45, "regular"}, headline = {32, "regular"}, title = {22, "regular"},
  title_medium = {16, "medium"}, body = {16, "regular"}, body_medium = {14, "regular"},
  body_small = {12, "regular"}, label = {14, "medium"}, label_small = {12, "medium"},
}
local function mix(a, b, amount)
  return {a[1] + (b[1] - a[1]) * amount, a[2] + (b[2] - a[2]) * amount,
          a[3] + (b[3] - a[3]) * amount, a[4]}
end
function M.new(theme)
  local colors = {}
  for key, value in pairs(palettes[theme] or palettes.dark) do colors[key] = value end
  for role, color in pairs(colors) do
    local next_color = {}
    for channel = 1, 4 do
      next_color[channel] = animation.value("material-color-" .. role .. "-" .. channel, color[channel],
        {duration = 0.3, bezier = {0.2, 0, 0, 1}, min = 0, max = 1})
    end
    colors[role] = next_color
  end
  local ui = {colors = colors, alpha = 1,
              font = os.getenv("VW_MATERIAL_FONT") or "Google Sans Flex"}
  function ui:scope(alpha, clip) self.alpha, self.clip = alpha or 1, clip end
  function ui:color(color)
    return {color[1], color[2], color[3], color[4] * math.max(0, math.min(1, self.alpha))}
  end
  function ui:text(value, x, y, role, color, width, lines)
    local style = type_scale[role or "body"] or type_scale.body
    return base.text({text = tostring(value or ""), x = x, y = y, font = self.font,
      size = style[1], weight = style[2], color = self:color(color or self.colors.on_surface),
      max_width = width or 0, max_lines = lines or 1, clip = self.clip, anchor = "top_left"})
  end
  function ui:rect(x, y, width, height, color, radius, callback, outline)
    local background = color or self.colors.container
    return base.rectangle({x = x, y = y, width = width, height = height, radius = radius or 16,
      color = self:color(background), clip = self.clip,
      border_width = outline and 1 or 0,
      border_color = self:color(outline or self.colors.outline_variant),
      on_click = callback, hover = callback and {
        color = self:color(mix(background, self.colors.on_surface, 0.08)), duration = 0.15} or nil})
  end
  function ui:icon(name, x, y, width, height, color, font_size)
    return base.stack({x = x, y = y, width = width, height = height, clip = self.clip,
      children = {base.text({text = name, font = "Material Symbols Rounded", size = font_size or 24,
        color = self:color(color or self.colors.on_surface_variant), anchor = "center"})}})
  end
  function ui:button(label, x, y, width, callback, variant, symbol)
    local C = self.colors
    local background, foreground = C.secondary_container, C.on_secondary_container
    if variant == "filled" then background, foreground = C.primary, C.on_primary
    elseif variant == "outlined" or variant == "text" then background, foreground = C.container, C.primary end
    local items = {}
    if symbol then items[#items + 1] = base.text({text = symbol, font = "Material Symbols Rounded", size = 18,
      color = self:color(foreground), anchor = "center"}) end
    items[#items + 1] = base.text({text = label, font = self.font, size = 14, weight = "medium",
      color = self:color(foreground), anchor = "center"})
    local content = base.row({height = 40, gap = 8, children = items})
    local rectangle = base.rectangle({width = width, height = 40, radius = 20,
      color = self:color(background), border_width = variant == "outlined" and 1 or 0,
      border_color = self:color(C.outline), on_click = callback,
      hover = {color = self:color(mix(background, foreground, 0.08)), duration = 0.15}})
    return base.stack({x = x, y = y, width = width, height = 40, clip = self.clip,
                       children = {rectangle, content}})
  end
  function ui:icon_button(name, x, y, callback, selected)
    local C = self.colors
    self:rect(x, y, 40, 40, selected and C.secondary_container or C.container, 20, callback)
    self:icon(name, x, y, 40, 40, selected and C.on_secondary_container or C.on_surface_variant)
  end
  function ui:switch(id, enabled, x, y, callback)
    local C = self.colors
    local progress = animation.value("material-switch-" .. id, enabled and 1 or 0,
      {duration = 0.15, bezier = {0.2, 0, 0, 1}, min = 0, max = 1})
    self:rect(x, y, 52, 32, enabled and C.primary or C.container_highest, 16, callback,
              not enabled and C.outline or nil)
    local diameter = 16 + progress * 8
    local cx = x + 16 + progress * 20
    self:rect(cx - diameter / 2, y + 16 - diameter / 2, diameter, diameter,
      enabled and C.on_primary or C.outline, diameter / 2)
  end
  function ui:slider(options)
    local C = self.colors
    local value = animation.value("material-slider-" .. options.id, options.value or 0,
      {duration = options.dragging and 0 or 0.2, bezier = {0.2, 0, 0, 1}, min = 0, max = 1})
    local x, y, width = options.x, options.y, math.max(40, options.width)
    local height, travel = 40, math.max(0, width - 40)
    local center_x = x + 20 + travel * value
    self:rect(x + 20, y + 16, width - 20, 8, C.container_highest, 4)
    self:rect(x, y, 40 + travel * value, height,
      options.enabled == false and C.container_highest or C.primary, 20)
    self:icon(options.icon or "volume_up", center_x - 20, y, 40, 40,
      options.enabled == false and C.on_surface_variant or C.on_primary)
    if options.enabled == false then return end
    local offset, initial = 0, options.value or 0
    local function update(event, dragging)
      local next_value = travel > 0 and math.max(0, math.min(1, (event.x - offset - x - 20) / travel)) or 0
      options.on_change(next_value, dragging)
    end
    base.drag({x = x, y = y - 4, width = width, height = 48, clip = self.clip,
      on_drag_start = function(event)
        offset = math.abs(event.x - center_x) < 20 and event.x - center_x or 0
        update(event, true)
      end,
      on_drag = function(event) update(event, true) end,
      on_drag_end = function(event)
        if event.cancelled then options.on_change(initial, false)
        else update(event, false) end
      end})
  end
  return ui
end
return M
