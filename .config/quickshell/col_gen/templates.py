"""
Jinja2 template rendering for color configs.
"""

import os
import re
from pathlib import Path
from jinja2 import Environment, FileSystemLoader, BaseLoader

TEMPLATES_DIR = Path(__file__).parent / "templates"

# Template output mappings (template_name -> output_path)
# Paths use ~ for home dir, expanded at runtime
TEMPLATE_OUTPUTS = {
    "qs_json.js": [
        "~/.config/quickshell/Colors.json",
        "~/.config/quickshell/lockscreen/Colors.json",
        "~/.config/quickshell/menu/Colors.json",
    ],
    "hypr-colrs.conf": ["~/.config/hypr/colors.conf"],
    "ghostty": ["~/.config/ghostty/themes/Matugen.conf"],
    "gtk.css": [
        "~/.config/gtk-3.0/colors.css",
        "~/.config/gtk-4.0/colors.css",
    ],
    "micro.micro": ["~/.config/micro/colorschemes/matugen.micro"],
}


def expand_path(path: str) -> Path:
    """Expand ~ and env vars in path."""
    return Path(os.path.expanduser(os.path.expandvars(path)))


def convert_matugen_syntax(template_content: str) -> str:
    """
    Convert matugen template syntax to Jinja2.
    
    Matugen uses:
      {{colors.<name>.default.hex}} -> {{ colors.<name>.hex }}
      {{colors.<name>.default.hex_stripped}} -> {{ colors.<name>.hex_stripped }}
      {{image}} -> {{ image }}
      {{mode}} -> {{ mode }}
      <* for name, value in colors *> -> {% for name, value in colors.items() %}
      <* endfor *> -> {% endfor %}
      {{value.default.hex}} -> {{ value.hex }} (inside loops)
    """
    content = template_content
    
    # Convert for loops FIRST (matugen uses <* *> syntax)
    content = re.sub(r'<\*\s*for\s+(\w+),\s*(\w+)\s+in\s+colors\s*\*>', 
                     r'{% for \1, \2 in colors.items() %}', content)
    content = re.sub(r'<\*\s*endfor\s*\*>', r'{% endfor %}', content)
    
    # Convert color references with .default.: colors.name.default.hex -> colors.name.hex
    # This handles both {{ colors.name.default.hex }} and {{colors.name.default.hex}}
    content = re.sub(
        r'\{\{\s*colors\.([a-z_]+)\.default\.(hex(?:_stripped)?)\s*\}\}',
        r'{{ colors.\1.\2 }}',
        content
    )
    
    # Convert loop variable refs: value.default.hex -> value.hex
    content = re.sub(
        r'\{\{\s*(\w+)\.default\.(hex(?:_stripped)?)\s*\}\}',
        r'{{ \1.\2 }}',
        content
    )
    
    # Convert simple vars (without adding extra spaces if already spaced)
    content = re.sub(r'\{\{image\}\}', r'{{ image }}', content)
    content = re.sub(r'\{\{mode\}\}', r'{{ mode }}', content)
    
    # Convert any remaining {{var}} to {{ var }}
    content = re.sub(r'\{\{(\w+)\}\}', r'{{ \1 }}', content)
    
    return content


def render_template(
    template_name: str,
    colors: dict,
    image_path: str,
    mode: str,
) -> str:
    """
    Render a single template with color data.
    
    Args:
        template_name: Name of template file in templates/
        colors: Dict of color names -> {hex, hex_stripped}
        image_path: Absolute path to source image
        mode: "dark" or "light"
    
    Returns:
        Rendered template string
    """
    template_path = TEMPLATES_DIR / template_name
    if not template_path.exists():
        raise FileNotFoundError(f"Template not found: {template_path}")
    
    # Read and convert template syntax
    raw_content = template_path.read_text()
    jinja_content = convert_matugen_syntax(raw_content)
    
    # Create Jinja env from string
    env = Environment(loader=BaseLoader())
    template = env.from_string(jinja_content)
    
    return template.render(
        colors=colors,
        image=image_path,
        mode=mode,
    )


def render_all(
    colors: dict,
    image_path: str,
    mode: str,
) -> dict[str, str]:
    """
    Render all templates.
    
    Returns:
        Dict of output_path -> rendered_content
    """
    results = {}
    
    for template_name, output_paths in TEMPLATE_OUTPUTS.items():
        template_path = TEMPLATES_DIR / template_name
        if not template_path.exists():
            continue
        
        try:
            rendered = render_template(template_name, colors, image_path, mode)
            for output_path in output_paths:
                results[output_path] = rendered
        except Exception as e:
            print(f"Error rendering {template_name}: {e}")
    
    return results


def write_outputs(rendered: dict[str, str]) -> list[str]:
    """
    Write rendered templates to their output paths.
    
    Returns:
        List of successfully written paths
    """
    written = []
    
    for output_path, content in rendered.items():
        path = expand_path(output_path)
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
            written.append(str(path))
        except Exception as e:
            print(f"Error writing {path}: {e}")
    
    return written
