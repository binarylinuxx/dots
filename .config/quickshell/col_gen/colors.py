"""
MD3 color generation from images using materialyoucolor.
"""

from pathlib import Path
from materialyoucolor.quantize import QuantizeCelebi
from materialyoucolor.hct import Hct
from materialyoucolor.score.score import Score
from materialyoucolor.dynamiccolor.material_dynamic_colors import MaterialDynamicColors
from materialyoucolor.scheme.scheme_tonal_spot import SchemeTonalSpot
from materialyoucolor.scheme.scheme_expressive import SchemeExpressive
from materialyoucolor.scheme.scheme_fidelity import SchemeFidelity
from materialyoucolor.scheme.scheme_fruit_salad import SchemeFruitSalad
from materialyoucolor.scheme.scheme_monochrome import SchemeMonochrome
from materialyoucolor.scheme.scheme_neutral import SchemeNeutral
from materialyoucolor.scheme.scheme_rainbow import SchemeRainbow
from materialyoucolor.scheme.scheme_vibrant import SchemeVibrant
from materialyoucolor.scheme.scheme_content import SchemeContent
from PIL import Image

SCHEME_MAP = {
    "tonal-spot": SchemeTonalSpot,
    "expressive": SchemeExpressive,
    "fidelity": SchemeFidelity,
    "fruit-salad": SchemeFruitSalad,
    "monochrome": SchemeMonochrome,
    "neutral": SchemeNeutral,
    "rainbow": SchemeRainbow,
    "vibrant": SchemeVibrant,
    "content": SchemeContent,
}

BACKGROUND_FILTERS = {
    "dark": {
        "background": 0.90,
        "surface": 0.88,
        "surface_dim": 0.90,
        "surface_bright": 0.78,
        "surface_container_lowest": 0.96,
        "surface_container_low": 0.92,
        "surface_container": 0.88,
        "surface_container_high": 0.84,
        "surface_container_highest": 0.80,
    },
    "light": {
        "background": 0.92,
        "surface": 0.94,
        "surface_dim": 0.84,
        "surface_bright": 0.98,
        "surface_container_lowest": 1.00,
        "surface_container_low": 0.96,
        "surface_container": 0.92,
        "surface_container_high": 0.88,
        "surface_container_highest": 0.84,
    },
}


def extract_seed_color(image_path: str | Path) -> int:
    """Extract dominant seed color (ARGB int) from image."""
    img = Image.open(image_path).convert("RGB")
    # Resize for faster quantization
    img.thumbnail((128, 128))
    pixels = list(img.getdata())
    # Convert to list of [R, G, B] lists
    rgb_pixels = [[r, g, b] for r, g, b in pixels]
    # Quantize and score
    quantized = QuantizeCelebi(rgb_pixels, 128)
    scored = Score.score(quantized)
    return scored[0] if scored else 0xFF4285F4  # fallback to Google blue


def argb_to_hex(argb: int) -> str:
    """Convert ARGB int to #rrggbb hex string."""
    r = (argb >> 16) & 0xFF
    g = (argb >> 8) & 0xFF
    b = argb & 0xFF
    return f"#{r:02x}{g:02x}{b:02x}"


def argb_to_hex_stripped(argb: int) -> str:
    """Convert ARGB int to rrggbb hex string (no #)."""
    r = (argb >> 16) & 0xFF
    g = (argb >> 8) & 0xFF
    b = argb & 0xFF
    return f"{r:02x}{g:02x}{b:02x}"


def hex_to_rgb(hex_color: str) -> tuple[int, int, int]:
    """Convert #rrggbb to RGB channels."""
    value = hex_color.lstrip("#")
    return int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16)


def rgb_to_color(r: int, g: int, b: int) -> dict:
    """Build a color dict from RGB channels."""
    hex_color = f"#{r:02x}{g:02x}{b:02x}"
    return {"hex": hex_color, "hex_stripped": hex_color[1:]}


def blend_rgb(color: tuple[int, int, int], target: tuple[int, int, int], amount: float) -> tuple[int, int, int]:
    """Blend RGB color toward target by amount in the 0..1 range."""
    return tuple(round(channel + (target_channel - channel) * amount) for channel, target_channel in zip(color, target))


def filter_background_colors(colors: dict, mode: str) -> dict:
    """Keep generated accents, but force background/surface roles near black or white."""
    mode_key = mode.lower()
    filters = BACKGROUND_FILTERS.get(mode_key)
    if not filters:
        return colors

    target = (0, 0, 0) if mode_key == "dark" else (255, 255, 255)
    filtered = dict(colors)

    for name, amount in filters.items():
        color = filtered.get(name)
        if not color:
            continue

        rgb = hex_to_rgb(color["hex"])
        filtered[name] = rgb_to_color(*blend_rgb(rgb, target, amount))

    return filtered


def generate_scheme_from_seed(
    seed_argb: int,
    mode: str = "dark",
    scheme_type: str = "tonal-spot",
    contrast: float = 0.0,
) -> dict:
    """Generate MD3 color scheme from a raw ARGB seed integer. No image I/O."""
    source_hct = Hct.from_int(seed_argb)
    is_dark = mode.lower() == "dark"
    scheme_class = SCHEME_MAP.get(scheme_type, SchemeTonalSpot)
    scheme = scheme_class(source_hct, is_dark, contrast)

    color_getters = {
        "primary": MaterialDynamicColors.primary,
        "on_primary": MaterialDynamicColors.onPrimary,
        "primary_container": MaterialDynamicColors.primaryContainer,
        "on_primary_container": MaterialDynamicColors.onPrimaryContainer,
        "primary_fixed": MaterialDynamicColors.primaryFixed,
        "primary_fixed_dim": MaterialDynamicColors.primaryFixedDim,
        "on_primary_fixed": MaterialDynamicColors.onPrimaryFixed,
        "on_primary_fixed_variant": MaterialDynamicColors.onPrimaryFixedVariant,
        "secondary": MaterialDynamicColors.secondary,
        "on_secondary": MaterialDynamicColors.onSecondary,
        "secondary_container": MaterialDynamicColors.secondaryContainer,
        "on_secondary_container": MaterialDynamicColors.onSecondaryContainer,
        "secondary_fixed": MaterialDynamicColors.secondaryFixed,
        "secondary_fixed_dim": MaterialDynamicColors.secondaryFixedDim,
        "on_secondary_fixed": MaterialDynamicColors.onSecondaryFixed,
        "on_secondary_fixed_variant": MaterialDynamicColors.onSecondaryFixedVariant,
        "tertiary": MaterialDynamicColors.tertiary,
        "on_tertiary": MaterialDynamicColors.onTertiary,
        "tertiary_container": MaterialDynamicColors.tertiaryContainer,
        "on_tertiary_container": MaterialDynamicColors.onTertiaryContainer,
        "tertiary_fixed": MaterialDynamicColors.tertiaryFixed,
        "tertiary_fixed_dim": MaterialDynamicColors.tertiaryFixedDim,
        "on_tertiary_fixed": MaterialDynamicColors.onTertiaryFixed,
        "on_tertiary_fixed_variant": MaterialDynamicColors.onTertiaryFixedVariant,
        "error": MaterialDynamicColors.error,
        "on_error": MaterialDynamicColors.onError,
        "error_container": MaterialDynamicColors.errorContainer,
        "on_error_container": MaterialDynamicColors.onErrorContainer,
        "surface": MaterialDynamicColors.surface,
        "on_surface": MaterialDynamicColors.onSurface,
        "on_surface_variant": MaterialDynamicColors.onSurfaceVariant,
        "surface_dim": MaterialDynamicColors.surfaceDim,
        "surface_bright": MaterialDynamicColors.surfaceBright,
        "surface_container_lowest": MaterialDynamicColors.surfaceContainerLowest,
        "surface_container_low": MaterialDynamicColors.surfaceContainerLow,
        "surface_container": MaterialDynamicColors.surfaceContainer,
        "surface_container_high": MaterialDynamicColors.surfaceContainerHigh,
        "surface_container_highest": MaterialDynamicColors.surfaceContainerHighest,
        "surface_variant": MaterialDynamicColors.surfaceVariant,
        "outline": MaterialDynamicColors.outline,
        "outline_variant": MaterialDynamicColors.outlineVariant,
        "shadow": MaterialDynamicColors.shadow,
        "scrim": MaterialDynamicColors.scrim,
        "inverse_surface": MaterialDynamicColors.inverseSurface,
        "inverse_on_surface": MaterialDynamicColors.inverseOnSurface,
        "inverse_primary": MaterialDynamicColors.inversePrimary,
        "background": MaterialDynamicColors.background,
        "on_background": MaterialDynamicColors.onBackground,
    }

    colors = {}
    for name, getter in color_getters.items():
        try:
            argb = getter.get_argb(scheme)
            colors[name] = {"hex": argb_to_hex(argb), "hex_stripped": argb_to_hex_stripped(argb)}
        except Exception:
            colors[name] = {"hex": "#000000", "hex_stripped": "000000"}
    return filter_background_colors(colors, mode)


def generate_scheme(
    image_path: str | Path,
    mode: str = "dark",
    scheme_type: str = "tonal-spot",
    contrast: float = 0.0,
) -> dict:
    """
    Generate MD3 color scheme from image.
    Returns:
        Dict with color names as keys, values are dicts with 'hex' and 'hex_stripped'
    """
    seed = extract_seed_color(image_path)
    source_hct = Hct.from_int(seed)

    is_dark = mode.lower() == "dark"
    scheme_class = SCHEME_MAP.get(scheme_type, SchemeTonalSpot)
    scheme = scheme_class(source_hct, is_dark, contrast)

    color_getters = {
        "primary": MaterialDynamicColors.primary,
        "on_primary": MaterialDynamicColors.onPrimary,
        "primary_container": MaterialDynamicColors.primaryContainer,
        "on_primary_container": MaterialDynamicColors.onPrimaryContainer,
        "primary_fixed": MaterialDynamicColors.primaryFixed,
        "primary_fixed_dim": MaterialDynamicColors.primaryFixedDim,
        "on_primary_fixed": MaterialDynamicColors.onPrimaryFixed,
        "on_primary_fixed_variant": MaterialDynamicColors.onPrimaryFixedVariant,
        "secondary": MaterialDynamicColors.secondary,
        "on_secondary": MaterialDynamicColors.onSecondary,
        "secondary_container": MaterialDynamicColors.secondaryContainer,
        "on_secondary_container": MaterialDynamicColors.onSecondaryContainer,
        "secondary_fixed": MaterialDynamicColors.secondaryFixed,
        "secondary_fixed_dim": MaterialDynamicColors.secondaryFixedDim,
        "on_secondary_fixed": MaterialDynamicColors.onSecondaryFixed,
        "on_secondary_fixed_variant": MaterialDynamicColors.onSecondaryFixedVariant,
        "tertiary": MaterialDynamicColors.tertiary,
        "on_tertiary": MaterialDynamicColors.onTertiary,
        "tertiary_container": MaterialDynamicColors.tertiaryContainer,
        "on_tertiary_container": MaterialDynamicColors.onTertiaryContainer,
        "tertiary_fixed": MaterialDynamicColors.tertiaryFixed,
        "tertiary_fixed_dim": MaterialDynamicColors.tertiaryFixedDim,
        "on_tertiary_fixed": MaterialDynamicColors.onTertiaryFixed,
        "on_tertiary_fixed_variant": MaterialDynamicColors.onTertiaryFixedVariant,
        "error": MaterialDynamicColors.error,
        "on_error": MaterialDynamicColors.onError,
        "error_container": MaterialDynamicColors.errorContainer,
        "on_error_container": MaterialDynamicColors.onErrorContainer,
        "surface": MaterialDynamicColors.surface,
        "on_surface": MaterialDynamicColors.onSurface,
        "on_surface_variant": MaterialDynamicColors.onSurfaceVariant,
        "surface_dim": MaterialDynamicColors.surfaceDim,
        "surface_bright": MaterialDynamicColors.surfaceBright,
        "surface_container_lowest": MaterialDynamicColors.surfaceContainerLowest,
        "surface_container_low": MaterialDynamicColors.surfaceContainerLow,
        "surface_container": MaterialDynamicColors.surfaceContainer,
        "surface_container_high": MaterialDynamicColors.surfaceContainerHigh,
        "surface_container_highest": MaterialDynamicColors.surfaceContainerHighest,
        "surface_variant": MaterialDynamicColors.surfaceVariant,
        "outline": MaterialDynamicColors.outline,
        "outline_variant": MaterialDynamicColors.outlineVariant,
        "shadow": MaterialDynamicColors.shadow,
        "scrim": MaterialDynamicColors.scrim,
        "inverse_surface": MaterialDynamicColors.inverseSurface,
        "inverse_on_surface": MaterialDynamicColors.inverseOnSurface,
        "inverse_primary": MaterialDynamicColors.inversePrimary,
        "background": MaterialDynamicColors.background,
        "on_background": MaterialDynamicColors.onBackground,
    }

    colors = {}
    for name, getter in color_getters.items():
        try:
            argb = getter.get_argb(scheme)
            colors[name] = {"hex": argb_to_hex(argb), "hex_stripped": argb_to_hex_stripped(argb)}
        except Exception:
            colors[name] = {"hex": "#000000", "hex_stripped": "000000"}
    return filter_background_colors(colors, mode)
