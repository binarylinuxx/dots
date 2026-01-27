// Colors.js Matugen Colors for quickshell

var colors = {
    "background": "#1a1110",
    "foreground": "#f1dedc",
    "primary": "#ffb3ad",
    "primary-fixed": "#ffdad6",
    "primary-fixed-dim": "#ffb3ad",
    "on-primary": "#571e1b",
    "on-primary-fixed": "#3b0908",
    "on-primary-fixed-variant": "#73332f",
    "primary-container": "#73332f",
    "on-primary-container": "#ffdad6",
    "secondary": "#e7bdb9",
    "secondary-fixed": "#ffdad6",
    "secondary-fixed-dim": "#e7bdb9",
    "on-secondary": "#442927",
    "on-secondary-fixed": "#2c1513",
    "on-secondary-fixed-variant": "#5d3f3d",
    "secondary-container": "#5d3f3d",
    "on-secondary-container": "#ffdad6",
    "tertiary": "#e1c28c",
    "tertiary-fixed": "#ffdea6",
    "tertiary-fixed-dim": "#e1c28c",
    "on-tertiary": "#402d04",
    "on-tertiary-fixed": "#261900",
    "on-tertiary-fixed-variant": "#584419",
    "tertiary-container": "#584419",
    "on-tertiary-container": "#ffdea6",
    "error": "#ffb4ab",
    "on-error": "#690005",
    "error-container": "#93000a",
    "on-error-container": "#ffdad6",
    "surface": "#1a1110",
    "on-surface": "#f1dedc",
    "on-surface-variant": "#d8c2bf",
    "outline": "#a08c8a",
    "outline-variant": "#534342",
    "shadow": "#000000",
    "scrim": "#000000",
    "inverse-surface": "#f1dedc",
    "inverse-on-surface": "#392e2d",
    "inverse-primary": "#904a45",
    "surface-dim": "#1a1110",
    "surface-bright": "#423735",
    "surface-container-lowest": "#140c0b",
    "surface-container-low": "#231918",
    "surface-container": "#271d1c",
    "surface-container-high": "#322827",
    "surface-container-highest": "#3d3231",
};

function getColors() {
    return colors;
}

function updateColors(newColors) {
    colors = Object.assign({}, colors, newColors);
}
