#!/usr/bin/env python3
"""
analyze.py - Smart widget placement using image analysis

Analyzes wallpaper to find calm/uniform areas suitable for widget placement.
Uses variance and edge detection to avoid busy regions.

Usage:
    uv run analyze.py <image_path> [options]

Options:
    --cols          Grid columns (default: 16)
    --rows          Grid rows (default: 9)
    --output, -o    Output file (default: $BLXSHELL_PATH/widget_suggestions.json)
    --verbose, -v   Verbose output
"""

import argparse
import json
import os
import sys
from pathlib import Path

# Add opencv-python to dependencies
try:
    import cv2
    import numpy as np
except ImportError:
    print(
        "Error: opencv-python required. Run: uv add opencv-python numpy",
        file=sys.stderr,
    )
    sys.exit(1)

SHELL_ROOT = Path(os.environ.get("BLXSHELL_PATH", Path(__file__).parent.parent))
DEFAULT_OUTPUT = SHELL_ROOT / "widget_suggestions.json"
WIDGETS_FILE = SHELL_ROOT / "widgets.json"


def analyze_image(image_path: str, cols: int, rows: int) -> np.ndarray:
    """
    Analyze image and return a grid of "calmness" scores.
    Lower score = calmer/more uniform area = better for widgets.
    """
    # Load image
    img = cv2.imread(image_path)
    if img is None:
        raise ValueError(f"Could not load image: {image_path}")

    height, width = img.shape[:2]
    cell_h = height // rows
    cell_w = width // cols

    # Convert to grayscale for analysis
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # Detect edges (busy areas have more edges)
    edges = cv2.Canny(gray, 50, 150)

    # Calculate scores for each grid cell
    scores = np.zeros((rows, cols))

    for r in range(rows):
        for c in range(cols):
            y1, y2 = r * cell_h, (r + 1) * cell_h
            x1, x2 = c * cell_w, (c + 1) * cell_w

            cell = gray[y1:y2, x1:x2]
            cell_edges = edges[y1:y2, x1:x2]

            # Score based on:
            # 1. Variance (uniform areas have low variance)
            variance = np.var(cell)

            # 2. Edge density (calm areas have fewer edges)
            edge_density = np.sum(cell_edges > 0) / cell_edges.size

            # Combined score (lower = better for widgets)
            scores[r, c] = variance * 0.3 + edge_density * 1000 * 0.7

    return scores


def find_best_position(
    scores: np.ndarray, width: int, height: int, exclude: list = None
) -> tuple:
    """
    Find the best position for a widget of given size.
    Returns (gridX, gridY) or None if no valid position.
    """
    rows, cols = scores.shape
    exclude = exclude or []

    best_score = float("inf")
    best_pos = None

    for r in range(rows - height + 1):
        for c in range(cols - width + 1):
            # Check if overlaps with excluded regions
            overlaps = False
            for ex in exclude:
                ex_x, ex_y, ex_w, ex_h = ex
                if not (
                    c + width <= ex_x
                    or c >= ex_x + ex_w
                    or r + height <= ex_y
                    or r >= ex_y + ex_h
                ):
                    overlaps = True
                    break

            if overlaps:
                continue

            # Calculate average score for this region
            region_score = np.mean(scores[r : r + height, c : c + width])

            if region_score < best_score:
                best_score = region_score
                best_pos = (c, r)

    return best_pos


def load_existing_widgets() -> tuple:
    """Load existing widgets and removed types from widgets.json."""
    widgets = []
    removed = []
    if WIDGETS_FILE.exists():
        try:
            with open(WIDGETS_FILE) as f:
                data = json.load(f)
                # Support both array format and object format with "removed" key
                if isinstance(data, list):
                    widgets = data
                elif isinstance(data, dict):
                    widgets = data.get("widgets", [])
                    removed = data.get("removed", [])
        except (json.JSONDecodeError, IOError):
            pass
    return widgets, removed


def suggest_widgets(
    image_path: str, cols: int, rows: int, verbose: bool = False
) -> list:
    """
    Analyze image and suggest widget placements.
    Preserves existing widget sizes from widgets.json.
    """
    scores = analyze_image(image_path, cols, rows)

    if verbose:
        print("Grid scores (lower = calmer):")
        for r in range(scores.shape[0]):
            print(" ".join(f"{scores[r, c]:6.1f}" for c in range(scores.shape[1])))
        print()

    # Load existing widgets - only use widgets that exist in widgets.json
    existing, _ = load_existing_widgets()
    existing_by_type = {w.get("type"): w for w in existing}
    widgets = []
    exclude = []

    # Only process widgets that exist in widgets.json (don't add defaults)
    widget_types = list(existing_by_type.keys())

    if not widget_types:
        return widgets

    for wtype in widget_types:
        w = existing_by_type[wtype].get("gridWidth", 4)
        h = existing_by_type[wtype].get("gridHeight", 3)

        pos = find_best_position(scores, w, h, exclude)
        if pos:
            gx, gy = pos
            widgets.append(
                {
                    "type": wtype,
                    "gridX": gx,
                    "gridY": gy,
                    "gridWidth": w,
                    "gridHeight": h,
                    "reason": f"score: {np.mean(scores[gy : gy + h, gx : gx + w]):.1f}",
                }
            )
            exclude.append((gx, gy, w, h))
        elif verbose:
            print(f"Warning: Could not place {wtype} widget")

    return widgets


def main():
    parser = argparse.ArgumentParser(
        description="Analyze wallpaper for smart widget placement",
    )
    parser.add_argument("image", help="Path to wallpaper image")
    parser.add_argument(
        "--cols", type=int, default=16, help="Grid columns (default: 16)"
    )
    parser.add_argument("--rows", type=int, default=9, help="Grid rows (default: 9)")
    parser.add_argument(
        "-o", "--output", default=str(DEFAULT_OUTPUT), help="Output JSON file"
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Apply suggestions directly to widgets.json",
    )

    args = parser.parse_args()

    # Validate image
    image_path = Path(args.image).expanduser().resolve()
    if not image_path.exists():
        print(f"Error: Image not found: {image_path}", file=sys.stderr)
        sys.exit(1)

    if args.verbose:
        print(f"Analyzing: {image_path}")
        print(f"Grid: {args.cols}x{args.rows}")

    try:
        widgets = suggest_widgets(str(image_path), args.cols, args.rows, args.verbose)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # Write output
    output_path = Path(args.output).expanduser()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    output_data = {
        "image": str(image_path),
        "grid": {"cols": args.cols, "rows": args.rows},
        "widgets": widgets,
    }

    with open(output_path, "w") as f:
        json.dump(output_data, f, indent=2)

    print(f"Wrote suggestions to: {output_path}")

    # Apply directly to widgets.json if requested
    if args.apply:
        widgets_data = []
        for i, w in enumerate(widgets):
            widgets_data.append(
                {
                    "id": f"w{i + 1}",
                    "type": w["type"],
                    "gridX": w["gridX"],
                    "gridY": w["gridY"],
                    "gridWidth": w["gridWidth"],
                    "gridHeight": w["gridHeight"],
                    "title": w["type"].capitalize(),
                }
            )

        with open(WIDGETS_FILE, "w") as f:
            json.dump(widgets_data, f)

        print(f"Applied to: {WIDGETS_FILE}")

    for w in widgets:
        print(
            f"  {w['type']}: ({w['gridX']},{w['gridY']}) {w['gridWidth']}x{w['gridHeight']} - {w['reason']}"
        )


if __name__ == "__main__":
    main()
