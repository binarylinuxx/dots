"""
Post-generation hooks - run shell commands after templates are written.
"""

import subprocess
import shutil

# Hooks to run after generation
# Each hook: (name, command, check_binary)
# check_binary: if set, only run if this binary exists
HOOKS = [
    ("hyprland", "hyprctl reload", "hyprctl"),
    ("ghostty", "pkill -SIGUSR2 ghostty", "ghostty"),
    ("gtk", 'gsettings set org.gnome.desktop.interface gtk-theme ""; gsettings set org.gnome.desktop.interface gtk-theme adw-gtk3-{mode}', "gsettings"),
]


def run_hooks(mode: str, verbose: bool = False) -> list[str]:
    """
    Run post-generation hooks.
    
    Args:
        mode: "dark" or "light" (used in gtk hook)
        verbose: Print hook output
    
    Returns:
        List of successfully executed hook names
    """
    executed = []
    
    for name, command, check_binary in HOOKS:
        # Check if required binary exists
        if check_binary and not shutil.which(check_binary):
            if verbose:
                print(f"Skipping {name} hook: {check_binary} not found")
            continue
        
        # Substitute mode in command
        cmd = command.format(mode=mode)
        
        try:
            result = subprocess.run(
                cmd,
                shell=True,
                capture_output=True,
                text=True,
                timeout=10,
            )
            if verbose:
                if result.stdout:
                    print(f"[{name}] {result.stdout.strip()}")
                if result.stderr:
                    print(f"[{name}] stderr: {result.stderr.strip()}")
            executed.append(name)
        except subprocess.TimeoutExpired:
            print(f"Hook {name} timed out")
        except Exception as e:
            print(f"Hook {name} failed: {e}")
    
    return executed
