// Colors.js Matugen Colors for quickshell

var colors = {
    "background": "#191113",
    "foreground": "#efdfe1",
    "primary": "#ffb1c7",
    "primary-fixed": "#ffd9e2",
    "primary-fixed-dim": "#ffb1c7",
    "on-primary": "#541d31",
    "on-primary-fixed": "#3a071c",
    "on-primary-fixed-variant": "#703347",
    "primary-container": "#703347",
    "on-primary-container": "#ffd9e2",
    "secondary": "#e3bdc6",
    "secondary-fixed": "#ffd9e2",
    "secondary-fixed-dim": "#e3bdc6",
    "on-secondary": "#422930",
    "on-secondary-fixed": "#2b151c",
    "on-secondary-fixed-variant": "#5b3f47",
    "secondary-container": "#5b3f47",
    "on-secondary-container": "#ffd9e2",
    "tertiary": "#eebd93",
    "tertiary-fixed": "#ffdcc1",
    "tertiary-fixed-dim": "#eebd93",
    "on-tertiary": "#47290b",
    "on-tertiary-fixed": "#2e1500",
    "on-tertiary-fixed-variant": "#613f1f",
    "tertiary-container": "#613f1f",
    "on-tertiary-container": "#ffdcc1",
    "error": "#ffb4ab",
    "on-error": "#690005",
    "error-container": "#93000a",
    "on-error-container": "#ffdad6",
    "surface": "#191113",
    "on-surface": "#efdfe1",
    "on-surface-variant": "#d5c2c5",
    "outline": "#9e8c90",
    "outline-variant": "#514346",
    "shadow": "#000000",
    "scrim": "#000000",
    "inverse-surface": "#efdfe1",
    "inverse-on-surface": "#372e30",
    "inverse-primary": "#8c4a5f",
    "surface-dim": "#191113",
    "surface-bright": "#413739",
    "surface-container-lowest": "#140c0e",
    "surface-container-low": "#22191b",
    "surface-container": "#261d1f",
    "surface-container-high": "#31282a",
    "surface-container-highest": "#3c3234",
};

function getColors() {
    return colors;
}

function updateColors(newColors) {
    colors = Object.assign({}, colors, newColors);
}
