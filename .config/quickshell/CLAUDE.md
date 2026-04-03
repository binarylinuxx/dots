# INFO
- Quickshell usually not uses qmldir for imports it has better way 'import qs.folder1.folder2 imports are relative starting from root file location'
- for logs use 'qs log' no need in restart quickshell works as an ipc and has hot on-change reload 

# PROJECT STRUCTURE
.
├── Colors.js
├── Colors.json
├── QUICKSHELL_REFERENCE.md
├── bar
│   ├── Bar.qml
│   └── widgets
│       ├── Audio.qml
│       ├── Battery.qml
│       ├── Clock.qml
│       ├── Colors.js
│       ├── Network.qml
│       ├── QuickButtons.qml
│       ├── SystemTray.qml
│       ├── Taskbar.qml
│       ├── TaskbarButton.qml
│       ├── UserProfile.qml
│       ├── Workspaces.qml
│       └── cat.png
├── col_gen
│   ├── __pycache__
│   │   ├── colors.cpython-314.pyc
│   │   ├── hooks.cpython-314.pyc
│   │   └── templates.cpython-314.pyc
│   ├── analyze
│   ├── analyze.py
│   ├── colors.py
│   ├── generate
│   ├── hooks.py
│   ├── main.py
│   ├── md3gen
│   ├── pyproject.toml
│   ├── templates
│   │   ├── ghostty
│   │   ├── gtk.css
│   │   ├── hypr-colrs.conf
│   │   ├── micro.micro
│   │   ├── qs_json.js
│   │   └── waybar.css
│   ├── templates.py
│   └── uv.lock
├── config.json
├── fonts
│   ├── FiraCodeNerdFont-Regular.ttf
│   ├── MaterialSymbolsOutlined.ttf
│   ├── MaterialSymbolsRounded.ttf
│   ├── Rubik-Bold.ttf
│   ├── Rubik-Medium.ttf
│   └── Rubik-Regular.ttf
├── launcher
│   ├── Launcher.qml
│   └── modes
│       └── emojis.json
├── menu
│   └── Colors.json
├── modules
│   ├── Background.qml
│   ├── BackgroundClock.qml
│   ├── BackgroundClock.qml.backup
│   ├── HotCornerTrigger.qml
│   ├── LockContext.qml
│   ├── LockSurface.qml
│   ├── LogoutButton.qml
│   ├── MaterialShape.qml
│   ├── NightLight.qml
│   └── PowerMenu.qml
├── notifications
│   └── Notifications.qml
├── services
│   ├── BatteryService.qml
│   ├── Gstate.qml
│   ├── NetworkManager.qml
│   ├── NotificationService.qml
│   ├── OsRelease.qml
│   ├── River.qml # MY experimental configs that you must ignore since they are unrelated
│   └── Sway.qml # MY experimental configs that you must ignore since they are unrelated
├── shell.qml
├── shell_river.qml
├── shell_sway.qml
├── shot-2026-01-30-16-33-50.png
├── shot-2026-01-31-14-05-37.png
├── widget_suggestions.json
├── widgets
│   ├── AudioOsd.qml
│   ├── Calendar.qml
│   ├── MaterialSymbol.qml # Material Symbol Font wrapper
│   ├── RiverTags.qml
│   ├── ScreenCorner.qml
│   ├── Settings.qml
│   ├── Sidebar.qml
│   ├── SidebarMediaPlayer.qml
│   ├── StyledDropdown.qml
│   ├── StyledSlider.qml
│   ├── StyledSlider.qml.bak
│   ├── SwayWorkspaces.qml
│   └── ToggleSwitch.qml
└── widgets.json

# TECHNICAL DETAILS
- not a git repo and not meant to be
- using python as parser are forbidden
