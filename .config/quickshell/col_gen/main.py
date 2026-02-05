#!/usr/bin/env python3
"""
col_gen - Material You color generator for quickshell

Usage:
    uv run main.py image <path> [options]
    
Options:
    -m, --mode          dark | light (default: dark)
    -s, --scheme        tonal-spot | expressive | fidelity | fruit-salad | 
                        monochrome | neutral | rainbow | vibrant | content
                        (default: tonal-spot)
    -c, --contrast      Contrast level -1.0 to 1.0 (default: 0.0)
    --no-hooks          Skip post-generation hooks
    -v, --verbose       Verbose output
"""

import argparse
import sys
from pathlib import Path

from colors import generate_scheme, SCHEME_MAP
from templates import render_all, write_outputs
from hooks import run_hooks


def main():
    parser = argparse.ArgumentParser(
        description="Generate Material You colors from an image",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    
    subparsers = parser.add_subparsers(dest="command", required=True)
    
    # image command
    image_parser = subparsers.add_parser("image", help="Generate colors from image")
    image_parser.add_argument("path", help="Path to image file")
    image_parser.add_argument(
        "-m", "--mode",
        choices=["dark", "light"],
        default="dark",
        help="Color mode (default: dark)",
    )
    image_parser.add_argument(
        "-s", "--scheme",
        choices=list(SCHEME_MAP.keys()),
        default="tonal-spot",
        help="Scheme type (default: tonal-spot)",
    )
    image_parser.add_argument(
        "-c", "--contrast",
        type=float,
        default=0.0,
        help="Contrast level -1.0 to 1.0 (default: 0.0)",
    )
    image_parser.add_argument(
        "--no-hooks",
        action="store_true",
        help="Skip post-generation hooks",
    )
    image_parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Verbose output",
    )
    
    args = parser.parse_args()
    
    if args.command == "image":
        image_path = Path(args.path).expanduser().resolve()
        
        if not image_path.exists():
            print(f"Error: Image not found: {image_path}", file=sys.stderr)
            sys.exit(1)
        
        if args.verbose:
            print(f"Generating colors from: {image_path}")
            print(f"Mode: {args.mode}, Scheme: {args.scheme}, Contrast: {args.contrast}")
        
        # Generate colors
        colors = generate_scheme(
            image_path,
            mode=args.mode,
            scheme_type=args.scheme,
            contrast=args.contrast,
        )
        
        if args.verbose:
            print(f"Generated {len(colors)} colors")
            print(f"Primary: {colors.get('primary', {}).get('hex', 'N/A')}")
        
        # Render templates
        rendered = render_all(colors, str(image_path), args.mode)
        
        if args.verbose:
            print(f"Rendered {len(rendered)} templates")
        
        # Write outputs
        written = write_outputs(rendered)
        
        if args.verbose:
            for path in written:
                print(f"Wrote: {path}")
        
        # Run hooks
        if not args.no_hooks:
            executed = run_hooks(args.mode, verbose=args.verbose)
            if args.verbose and executed:
                print(f"Executed hooks: {', '.join(executed)}")
        
        print(f"Done. Generated {len(colors)} colors, wrote {len(written)} files.")


if __name__ == "__main__":
    main()
