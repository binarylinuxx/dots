"""
Static color themes for the col_gen pipeline.
"""

import json
import os
from pathlib import Path

from templates import SHELL_ROOT


XDG_CONFIG = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))

THEMES_DIRS = [
    Path.home() / ".local/blxshell/themes",
    XDG_CONFIG / "blxshell/themes",
    XDG_CONFIG / "quickshell/themes",
    SHELL_ROOT / "themes",
    Path(__file__).parent.parent / "themes",
]

TOKEN_MAP = {
    "background": "background",
    "foreground": "on_background",
    "primary": "primary",
    "primaryFixed": "primary_fixed",
    "primaryFixedDim": "primary_fixed_dim",
    "onPrimary": "on_primary",
    "onPrimaryFixed": "on_primary_fixed",
    "onPrimaryFixedVariant": "on_primary_fixed_variant",
    "primaryContainer": "primary_container",
    "onPrimaryContainer": "on_primary_container",
    "secondary": "secondary",
    "secondaryFixed": "secondary_fixed",
    "secondaryFixedDim": "secondary_fixed_dim",
    "onSecondary": "on_secondary",
    "onSecondaryFixed": "on_secondary_fixed",
    "onSecondaryFixedVariant": "on_secondary_fixed_variant",
    "secondaryContainer": "secondary_container",
    "onSecondaryContainer": "on_secondary_container",
    "tertiary": "tertiary",
    "tertiaryFixed": "tertiary_fixed",
    "tertiaryFixedDim": "tertiary_fixed_dim",
    "onTertiary": "on_tertiary",
    "onTertiaryFixed": "on_tertiary_fixed",
    "onTertiaryFixedVariant": "on_tertiary_fixed_variant",
    "tertiaryContainer": "tertiary_container",
    "onTertiaryContainer": "on_tertiary_container",
    "error": "error",
    "onError": "on_error",
    "errorContainer": "error_container",
    "onErrorContainer": "on_error_container",
    "surface": "surface",
    "onSurface": "on_surface",
    "onSurfaceVariant": "on_surface_variant",
    "outline": "outline",
    "outlineVariant": "outline_variant",
    "shadow": "shadow",
    "scrim": "scrim",
    "inverseSurface": "inverse_surface",
    "inverseOnSurface": "inverse_on_surface",
    "inversePrimary": "inverse_primary",
    "surfaceDim": "surface_dim",
    "surfaceBright": "surface_bright",
    "surfaceContainerLowest": "surface_container_lowest",
    "surfaceContainerLow": "surface_container_low",
    "surfaceContainer": "surface_container",
    "surfaceContainerHigh": "surface_container_high",
    "surfaceContainerHighest": "surface_container_highest",
    "surfaceVariant": "surface_variant",
}


def _color_value(value: str) -> dict[str, str]:
    hex_value = value.lower()
    if not hex_value.startswith("#"):
        hex_value = "#" + hex_value
    return {"hex": hex_value, "hex_stripped": hex_value[1:]}


def list_static_themes() -> list[str]:
    names = set()
    for themes_dir in THEMES_DIRS:
        if themes_dir.exists():
            names.update(path.stem for path in themes_dir.glob("*.json"))
    return sorted(names)


def load_static_theme(name: str) -> tuple[dict, str]:
    theme_path = next((path for path in (themes_dir / f"{name}.json" for themes_dir in THEMES_DIRS) if path.exists()), None)
    if theme_path is None:
        available = ", ".join(list_static_themes()) or "none"
        raise FileNotFoundError(f"Static theme not found: {name}. Available: {available}")

    raw = json.loads(theme_path.read_text())
    colors = {}
    for source_name, target_name in TOKEN_MAP.items():
        value = raw.get(source_name)
        if isinstance(value, str) and value.startswith("#"):
            colors[target_name] = _color_value(value)

    if "on_background" not in colors and "on_surface" in colors:
        colors["on_background"] = colors["on_surface"]
    if "surface_variant" not in colors and "surface_container_highest" in colors:
        colors["surface_variant"] = colors["surface_container_highest"]

    mode = raw.get("mode")
    if mode not in ("dark", "light"):
        mode = "light" if name.endswith("-light") else "dark"

    return colors, mode
