// Colors.js Matugen Colors for quickshell

var colors = {
    "background": "#111318",
    "foreground": "#e2e2e9",
    "primary": "#a9c7ff",
    "primary-fixed": "#d6e3ff",
    "primary-fixed-dim": "#a9c7ff",
    "on-primary": "#09305f",
    "on-primary-fixed": "#001b3d",
    "on-primary-fixed-variant": "#274777",
    "primary-container": "#274777",
    "on-primary-container": "#d6e3ff",
    "secondary": "#bdc7dc",
    "secondary-fixed": "#dae2f9",
    "secondary-fixed-dim": "#bdc7dc",
    "on-secondary": "#283141",
    "on-secondary-fixed": "#121c2b",
    "on-secondary-fixed-variant": "#3e4758",
    "secondary-container": "#3e4758",
    "on-secondary-container": "#dae2f9",
    "tertiary": "#dcbce1",
    "tertiary-fixed": "#f9d8fd",
    "tertiary-fixed-dim": "#dcbce1",
    "on-tertiary": "#3f2845",
    "on-tertiary-fixed": "#28132e",
    "on-tertiary-fixed-variant": "#563e5c",
    "tertiary-container": "#563e5c",
    "on-tertiary-container": "#f9d8fd",
    "error": "#ffb4ab",
    "on-error": "#690005",
    "error-container": "#93000a",
    "on-error-container": "#ffdad6",
    "surface": "#111318",
    "on-surface": "#e2e2e9",
    "on-surface-variant": "#c4c6cf",
    "outline": "#8e9099",
    "outline-variant": "#44474e",
    "shadow": "#000000",
    "scrim": "#000000",
    "inverse-surface": "#e2e2e9",
    "inverse-on-surface": "#2e3036",
    "inverse-primary": "#405f90",
    "surface-dim": "#111318",
    "surface-bright": "#37393e",
    "surface-container-lowest": "#0c0e13",
    "surface-container-low": "#191c20",
    "surface-container": "#1d2024",
    "surface-container-high": "#282a2f",
    "surface-container-highest": "#33353a",
};

function getColors() {
    return colors;
}

function updateColors(newColors) {
    colors = Object.assign({}, colors, newColors);
}
