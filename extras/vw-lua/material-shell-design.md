# Material shell design notes

This is a native Material 3-inspired desktop shell, not Material Web running in a
browser and not a claim of complete Material component/accessibility parity.

## Research sources

Google's design site requires JavaScript; the readable official implementation
documentation and token files below were used to verify sizes and behavior:

- [Material Web color roles](https://github.com/material-components/material-web/blob/main/docs/theming/color.md)
- [Material Web typography](https://github.com/material-components/material-web/blob/main/docs/theming/typography.md)
- [Generated type scale](https://github.com/material-components/material-web/blob/main/tokens/versions/v0_192/_md-sys-typescale.scss)
- [Generated motion tokens](https://github.com/material-components/material-web/blob/main/tokens/versions/v0_192/_md-sys-motion.scss)
- [Button types](https://github.com/material-components/material-web/blob/main/docs/components/button.md)
- [Filled button tokens](https://github.com/material-components/material-web/blob/main/tokens/versions/v0_192/_md-comp-filled-button.scss)
- [FAB guidance](https://github.com/material-components/material-web/blob/main/docs/components/fab.md)
- [Slider guidance](https://github.com/material-components/material-web/blob/main/docs/components/slider.md)
- [Android navigation rail dimensions](https://github.com/material-components/material-components-android/blob/master/lib/java/com/google/android/material/navigationrail/res/values/dimens.xml)

## Decisions

- Semantic surface/container and paired on-color roles; elevation is conveyed
  through tonal surfaces rather than decorative gradients or pervasive outlines.
- Blue primary palette with dark/light neutral surfaces. These are curated roles,
  not a runtime wallpaper-derived Material Color Utilities palette.
- Google Sans Flex for branding/body, Material Symbols Rounded for icons. This
  adapts the official brand/plain distinction; Roboto is Material Web's default.
- Display 45, headline 32, title 22, body 16/14/12, label 14/12 pixels.
- 40px fully rounded common buttons, 56px rounded FAB, 24px navigation icons,
  selected tonal rail indicators, and generous 8/12/16/24 spacing.
- Desktop rail is 80px wide; this is an adaptation, not an assertion that every
  current Android/web rail token specifies the same width.
- 300ms emphasized `(0.2, 0, 0, 1)` panel motion and 150ms hover/state transitions,
  driven by compositor frames. Panel content is clipped during reveals.
- The thick, icon-bearing slider follows the requested expressive visual
  reference and uses primary/inactive surface roles; it is not the 4px baseline
  web slider reproduced verbatim.
- The frame, rail, toolbar, and open panel are rounded rectangles on one surface.
  Semantic colors distinguish the component layers.
- One fullscreen layer surface, with only the frame, rail, toolbar, and open panel
  accepting input. The desktop center is click-through. No fake weather, network
  toggles, or notification history from another daemon.

Native backend: zwwm/Hyprland, GIO application icons, MPRIS, and PipeWire. System
settings shortcuts launch installed applications; they do not pretend to expose
unimplemented NetworkManager/Bluetooth services.
