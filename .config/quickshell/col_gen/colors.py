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


def generate_scheme(
    image_path: str | Path,
    mode: str = "dark",
    scheme_type: str = "tonal-spot",
    contrast: float = 0.0,
) -> dict:
    """
    Generate MD3 color scheme from image.
    
    Args:
        image_path: Path to source image
        mode: "dark" or "light"
        scheme_type: One of SCHEME_MAP keys
        contrast: Contrast level (-1.0 to 1.0)
    
    Returns:
        Dict with color names as keys, values are dicts with 'hex' and 'hex_stripped'
    """
    seed = extract_seed_color(image_path)
    source_hct = Hct.from_int(seed)
    
    is_dark = mode.lower() == "dark"
    scheme_class = SCHEME_MAP.get(scheme_type, SchemeTonalSpot)
    scheme = scheme_class(source_hct, is_dark, contrast)
    
    # Map MaterialDynamicColors attributes (camelCase) to output names (snake_case)
    # The attribute names use camelCase, output keys use snake_case for template compatibility
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
            colors[name] = {
                "hex": argb_to_hex(argb),
                "hex_stripped": argb_to_hex_stripped(argb),
            }
        except Exception:
            # Fallback for missing colors
            colors[name] = {"hex": "#000000", "hex_stripped": "000000"}
    
    return colors
