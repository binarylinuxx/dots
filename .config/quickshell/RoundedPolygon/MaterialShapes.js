.pragma library

.import "RoundedPolygon.js" as RP
.import "Matrix.js" as MX

// MaterialShapes.js
// All 35 Google Material 3 shapes.
// Coordinates and transform sequences are verbatim from the reference
// (end-4/rounded-polygon-qmljs / material-shapes.js).
//
// Key rule: transforms are applied BEFORE .normalized(), in raw polygon space
// (centre ≈ 0,0), using Matrix.rotateZ which rotates around (0,0) exactly
// as the reference Matrix does.
//
// Public:
//   get(typeName)  →  { cubics }   (normalised to [0,1]²)
//   names()        →  String[]

// ─── rounding constructors ────────────────────────────────────────────────────

function r(radius, smoothing) {
    return { radius: radius || 0, smoothing: smoothing || 0 };
}
var U = r(0, 0);   // Unrounded — matches CornerRounding.Unrounded in reference

function rAll(n, radius, smoothing) {
    var a = [];
    for (var i = 0; i < n; i++) a.push(r(radius, smoothing || 0));
    return a;
}

// pre-built named matrices matching reference module-level vars
var rotateNeg30  = MX.makeRotateZ(-30);
var rotateNeg45  = MX.makeRotateZ(-45);
var rotateNeg90  = MX.makeRotateZ(-90);
var rotateNeg135 = MX.makeRotateZ(-135);
var rotate30     = MX.makeRotateZ(30);
var rotate45     = MX.makeRotateZ(45);
var rotate60     = MX.makeRotateZ(60);
var rotate90     = MX.makeRotateZ(90);
var rotate120    = MX.makeRotateZ(120);
var rotate135    = MX.makeRotateZ(135);
var rotate180    = MX.makeRotateZ(180);
var rotate28th   = MX.makeRotateZ(360 / 28);
var rotateNeg16th= MX.makeRotateZ(-360 / 16);

function T(shape, matrix) { return MX.applyToShape(shape, matrix); }
function N(shape)          { return RP.normalizedShape(shape); }

// ─── PointNRound helpers ──────────────────────────────────────────────────────

function pt(x, y, rnd) { return { x: x, y: y, r: rnd || U }; }

function fromPNR(list) {
    var verts = [], pvr = [];
    for (var i = 0; i < list.length; i++) {
        verts.push(list[i].x, list[i].y);
        pvr.push(list[i].r);
    }
    return RP.buildShape(verts, pvr);   // NOT normalized yet
}

// ─── doRepeat — exact port of reference doRepeat ─────────────────────────────

function doRepeat(specs, reps, mirroring, cx, cy) {
    cx = (cx === undefined) ? 0.5 : cx;
    cy = (cy === undefined) ? 0.5 : cy;

    if (mirroring) {
        var angles = specs.map(function(s) {
            return Math.atan2(s.y - cy, s.x - cx) * 180 / Math.PI;
        });
        var dists = specs.map(function(s) {
            var dx = s.x - cx, dy = s.y - cy;
            return Math.sqrt(dx * dx + dy * dy);
        });
        var actualReps   = reps * 2;
        var sectionAngle = 360 / actualReps;
        var result       = [];
        for (var it = 0; it < actualReps; it++) {
            for (var index = 0; index < specs.length; index++) {
                var i = (it % 2 === 0) ? index : specs.length - 1 - index;
                if (i > 0 || it % 2 === 0) {
                    var baseAngle = angles[i];
                    var angle = it * sectionAngle + (it % 2 === 0
                        ? baseAngle
                        : (2 * angles[0] - baseAngle));
                    var rad = angle * Math.PI / 180;
                    result.push(pt(cx + Math.cos(rad) * dists[i],
                                   cy + Math.sin(rad) * dists[i],
                                   specs[i].r));
                }
            }
        }
        return result;
    }

    // simple rotation
    var np = specs.length, out = [];
    for (var j = 0; j < np * reps; j++) {
        var src = specs[j % np];
        var deg = Math.floor(j / np) * 360 / reps;
        var t   = deg * Math.PI / 180;
        var co  = Math.cos(t), si = Math.sin(t);
        var ox  = src.x - cx, oy = src.y - cy;
        out.push(pt(cx + ox * co - oy * si,
                    cy + ox * si + oy * co,
                    src.r));
    }
    return out;
}

// Build shape from customPolygon spec, NOT normalized (so transforms can follow)
function customPolygonRaw(specs, reps, mirroring) {
    return fromPNR(doRepeat(specs, reps || 1, !!mirroring));
}
// Build + normalize (for shapes with no post-normalize transforms)
function customPolygon(specs, reps, mirroring) {
    return N(customPolygonRaw(specs, reps, mirroring));
}

// ─── 35 shapes, exactly matching reference transform sequences ────────────────

function makeCircle() {
    // RoundedPolygon.circle(10).transformed(rotate45).normalized()
    return N(T(RP.buildCircleRaw(10, 1, 0, 0), rotate45));
}

function makeSquare() {
    return N(RP.buildRectRaw(1, 1, rAll(4, 0.3), 0, 0));
}

function makeSlanted() {
    return customPolygon([
        pt(0.926, 0.970, r(0.189, 0.811)),
        pt(-0.021, 0.967, r(0.187, 0.057))
    ], 2);
}

function makeArch() {
    return N(RP.buildRectRaw(1, 1, [r(0.2), r(0.2), r(1.0), r(1.0)], 0, 0));
}

function makeFan() {
    return customPolygon([
        pt(1.004, 1.000, r(0.148, 0.417)),
        pt(0.000, 1.000, r(0.151)),
        pt(0.000,-0.003, r(0.148)),
        pt(0.978, 0.020, r(0.803))
    ], 1);
}

function makeArrow() {
    return customPolygon([
        pt(1.225, 1.060, r(0.211)),
        pt(0.500, 0.892, r(0.313)),
        pt(-0.216, 1.050, r(0.207)),
        pt(0.499,-0.160, r(0.215, 1.000))
    ], 1);
}

function makeSemiCircle() {
    return N(RP.buildRectRaw(1.6, 1.0, [r(0.2), r(0.2), r(1.0), r(1.0)], 0, 0));
}

function makeOval() {
    // circle().transformed(rotateNeg90).transformed(scale(1,0.64)).transformed(rotate135).normalized()
    var scaleMatrix = MX.makeScale(1, 0.64);
    var s = RP.buildCircleRaw(8, 1, 0, 0);
    s = T(s, rotateNeg90);
    s = T(s, scaleMatrix);
    s = T(s, rotate135);
    return N(s);
}

function makePill() {
    // customPolygon([...], 2).transformed(rotate180).normalized()
    var s = customPolygonRaw([
        pt(0.428,-0.001, r(0.426)),
        pt(0.961, 0.039, r(0.426)),
        pt(1.001, 0.428, U),
        pt(1.000, 0.609, r(1.000))
    ], 2);
    return N(T(s, rotate180));
}

function makeTriangle() {
    // fromNumVertices(3, 1, 0.5, 0.5, r20).transformed(rotate30).normalized()
    var s = RP.buildRegularPolygonRaw(3, 1, 0.5, 0.5, rAll(3, 0.2));
    return N(T(s, rotate30));
}

function makeDiamond() {
    return customPolygon([
        pt(0.500, 1.096, r(0.151, 0.524)),
        pt(0.040, 0.500, r(0.159))
    ], 2);
}

function makeClamShell() {
    return customPolygon([
        pt(0.829, 0.841, r(0.159)),
        pt(0.171, 0.841, r(0.159)),
        pt(-0.020, 0.500, r(0.140))
    ], 2);
}

function makePentagon() {
    return customPolygon([
        pt(0.828, 0.970, r(0.169)),
        pt(0.172, 0.970, r(0.169)),
        pt(-0.030, 0.365, r(0.164)),
        pt(0.500,-0.009, r(0.172)),
        pt(1.030, 0.365, r(0.164))
    ], 1);
}

function makeGem() {
    return customPolygon([
        pt(1.005, 0.792, r(0.208)),
        pt(0.500, 1.023, r(0.241, 0.778)),
        pt(-0.005, 0.792, r(0.208)),
        pt(0.073, 0.258, r(0.228)),
        pt(0.500, 0.000, r(0.241, 0.778)),
        pt(0.927, 0.258, r(0.228))
    ], 1);
}

function makeSunny() {
    // star(8, 1, 0.8, r15).transformed(rotate45).normalized()
    var s = RP.buildStarRaw(8, 1, 0.8, rAll(16, 0.15), 0, 0);
    return N(T(s, rotate45));
}

function makeVerySunny() {
    var s = customPolygonRaw([
        pt(0.500, 1.080, r(0.085)),
        pt(0.358, 0.843, r(0.085))
    ], 8);
    return N(T(s, rotateNeg45));
}

function makeCookie4() {
    return customPolygon([
        pt(1.237, 1.236, r(0.258)),
        pt(0.500, 0.918, r(0.233))
    ], 4);
}

function makeCookie6() {
    return customPolygon([
        pt(0.723, 0.884, r(0.394)),
        pt(0.500, 1.099, r(0.398))
    ], 6);
}

function makeCookie7() {
    // star(7,1,0.75,r50).normalized().transformed(rotate28th)×5.normalized()
    var s = RP.buildStarRaw(7, 1, 0.75, rAll(14, 0.5), 0, 0);
    s = N(s);
    s = T(s, rotate28th);
    s = T(s, rotate28th);
    s = T(s, rotate28th);
    s = T(s, rotate28th);
    s = T(s, rotate28th);
    return N(s);
}

function makeCookie9() {
    var s = RP.buildStarRaw(9, 1, 0.8, rAll(18, 0.5), 0, 0);
    return N(T(s, rotate30));
}

function makeCookie12() {
    var s = RP.buildStarRaw(12, 1, 0.8, rAll(24, 0.5), 0, 0);
    return N(T(s, rotate30));
}

function makeGhostish() {
    return customPolygon([
        pt(1.000, 1.140, r(0.254, 0.106)),
        pt(0.575, 0.906, r(0.253)),
        pt(0.425, 0.906, r(0.253)),
        pt(0.000, 1.140, r(0.254, 0.106)),
        pt(0.000, 0.000, r(1.0)),
        pt(0.500, 0.000, r(1.0)),
        pt(1.000, 0.000, r(1.0))
    ], 1);
}

function makeClover4() {
    return customPolygon([
        pt(1.099, 0.725, r(0.476)),
        pt(0.725, 1.099, r(0.476)),
        pt(0.500, 0.926, U)
    ], 4);
}

function makeClover8() {
    return customPolygon([
        pt(0.758, 1.101, r(0.209)),
        pt(0.500, 0.964, U)
    ], 8);
}

function makeBurst() {
    var s = customPolygonRaw([
        pt(0.592, 0.842, r(0.006)),
        pt(0.500, 1.006, r(0.006))
    ], 12);
    s = T(s, rotateNeg30);
    s = T(s, rotateNeg30);
    return N(s);
}

function makeSoftBurst() {
    var s = customPolygonRaw([
        pt(0.193, 0.277, r(0.053)),
        pt(0.176, 0.055, r(0.053))
    ], 10);
    return N(T(s, rotate180));
}

function makeBoom() {
    var s = customPolygonRaw([
        pt(0.457, 0.296, r(0.007)),
        pt(0.500,-0.051, r(0.007))
    ], 15);
    return N(T(s, rotate120));
}

function makeSoftBoom() {
    var s = customPolygonRaw([
        pt(0.733, 0.454, U),
        pt(0.839, 0.437, r(0.532)),
        pt(0.949, 0.449, r(0.439, 1.0)),
        pt(0.998, 0.478, r(0.174)),
        pt(0.998, 0.522, r(0.174)),
        pt(0.949, 0.551, r(0.439, 1.0)),
        pt(0.839, 0.563, r(0.532)),
        pt(0.733, 0.546, U)
    ], 16);
    s = T(s, rotate45);
    s = T(s, rotateNeg16th);
    return N(s);
}

function makeFlower() {
    var s = customPolygonRaw([
        pt(0.370, 0.187, U),
        pt(0.416, 0.049, r(0.381)),
        pt(0.479, 0.001, r(0.095)),
        pt(0.521, 0.001, r(0.095)),
        pt(0.584, 0.049, r(0.381)),
        pt(0.630, 0.187, U)
    ], 8);
    return N(T(s, rotate135));
}

function makePuffy() {
    var scaleM = MX.makeScale(1, 0.742);
    var s = customPolygonRaw([
        pt(1.003, 0.563, r(0.255)),
        pt(0.940, 0.656, r(0.126)),
        pt(0.881, 0.654, U),
        pt(0.926, 0.711, r(0.660)),
        pt(0.914, 0.851, r(0.660)),
        pt(0.777, 0.998, r(0.360)),
        pt(0.722, 0.872, U),
        pt(0.717, 0.934, r(0.574)),
        pt(0.670, 1.035, r(0.426)),
        pt(0.545, 1.040, r(0.405)),
        pt(0.500, 0.947, U),
        pt(0.500, 1-0.053, U),
        pt(1-0.545, 1+0.040, r(0.405)),
        pt(1-0.670, 1+0.035, r(0.426)),
        pt(1-0.717, 1-0.066, r(0.574)),
        pt(1-0.722, 1-0.128, U),
        pt(1-0.777, 1-0.002, r(0.360)),
        pt(1-0.914, 1-0.149, r(0.660)),
        pt(1-0.926, 1-0.289, r(0.660)),
        pt(1-0.881, 1-0.346, U),
        pt(1-0.940, 1-0.344, r(0.126)),
        pt(1-1.003, 1-0.437, r(0.255))
    ], 2);
    return N(T(s, scaleM));
}

function makePuffyDiamond() {
    var s = customPolygonRaw([
        pt(0.870, 0.130, r(0.146)),
        pt(0.818, 0.357, U),
        pt(1.000, 0.332, r(0.853)),
        pt(1.000, 1-0.332, r(0.853)),
        pt(0.818, 1-0.357, U)
    ], 4);
    return N(T(s, rotate90));
}

function makeCloud() {
    return customPolygon([
        pt(0.3636, 0.2727, r(1.0)),
        pt(0.6364, 0.2727, r(1.0)),
        pt(0.6364, 0.4545, r(1.0)),
        pt(0.8182, 0.4545, r(1.0)),
        pt(0.8182, 0.5455, r(1.0)),
        pt(0.9091, 0.5455, r(1.0)),
        pt(0.9091, 0.7273, r(1.0)),
        pt(0.0000, 0.7273, r(1.0)),
        pt(0.0000, 0.4545, r(1.0)),
        pt(0.0909, 0.4545, r(1.0)),
        pt(0.0909, 0.3636, r(1.0)),
        pt(0.1818, 0.3636, r(1.0)),
        pt(0.1818, 0.4545, r(1.0)),
        pt(0.3636, 0.4545, r(1.0))
    ], 1);
}

function makePixelCircle() {
    return customPolygon([
        pt(1.000, 0.704, U), pt(0.926, 0.704, U),
        pt(0.926, 0.852, U), pt(0.843, 0.852, U),
        pt(0.843, 0.935, U), pt(0.704, 0.935, U),
        pt(0.704, 1.000, U), pt(0.500, 1.000, U),
        pt(1-0.704, 1.000, U), pt(1-0.704, 0.935, U),
        pt(1-0.843, 0.935, U), pt(1-0.843, 0.852, U),
        pt(1-0.926, 0.852, U), pt(1-0.926, 0.704, U),
        pt(1-1.000, 0.704, U)
    ], 2);
}

function makePixelTriangle() {
    return customPolygon([
        pt(0.888, 1-0.439, U), pt(0.789, 1-0.439, U),
        pt(0.789, 1-0.344, U), pt(0.675, 1-0.344, U),
        pt(0.674, 1-0.265, U), pt(0.560, 1-0.265, U),
        pt(0.560, 1-0.170, U), pt(0.421, 1-0.170, U),
        pt(0.421, 1-0.087, U), pt(0.287, 1-0.087, U),
        pt(0.287, 1-0.000, U), pt(0.113, 1-0.000, U),
        pt(0.110, 0.500,    U), pt(0.113, 0.000,    U),
        pt(0.287, 0.000,    U), pt(0.287, 0.087,    U),
        pt(0.421, 0.087,    U), pt(0.421, 0.170,    U),
        pt(0.560, 0.170,    U), pt(0.560, 0.265,    U),
        pt(0.674, 0.265,    U), pt(0.675, 0.344,    U),
        pt(0.789, 0.344,    U), pt(0.789, 0.439,    U),
        pt(0.888, 0.439,    U)
    ], 1);
}

function makeBun() {
    return customPolygon([
        pt(0.796, 0.500, U),
        pt(0.853, 0.518, r(1.0)),
        pt(0.992, 0.631, r(1.0)),
        pt(0.968, 1.000, r(1.0)),
        pt(0.032, 1-0.000, r(1.0)),
        pt(0.008, 1-0.369, r(1.0)),
        pt(0.147, 1-0.482, r(1.0)),
        pt(0.204, 1-0.500, U)
    ], 2);
}

function makeHeart() {
    return customPolygon([
        pt(0.782,  0.611,  U),
        pt(0.499,  0.946,  r(0.000)),
        pt(0.2175, 0.611,  U),
        pt(-0.064, 0.276,  r(1.000)),
        pt(0.208, -0.066,  r(0.958)),
        pt(0.500,  0.268,  r(0.016)),
        pt(0.792, -0.066,  r(0.958)),
        pt(1.064,  0.276,  r(1.000))
    ], 1);
}

// ─── registry & cache ─────────────────────────────────────────────────────────

var _builders = {
    "circle":        makeCircle,
    "square":        makeSquare,
    "slanted":       makeSlanted,
    "arch":          makeArch,
    "fan":           makeFan,
    "arrow":         makeArrow,
    "semicircle":    makeSemiCircle,
    "oval":          makeOval,
    "pill":          makePill,
    "triangle":      makeTriangle,
    "diamond":       makeDiamond,
    "clamshell":     makeClamShell,
    "pentagon":      makePentagon,
    "gem":           makeGem,
    "sunny":         makeSunny,
    "verysunny":     makeVerySunny,
    "cookie4sided":  makeCookie4,
    "cookie6sided":  makeCookie6,
    "cookie7sided":  makeCookie7,
    "cookie9sided":  makeCookie9,
    "cookie12sided": makeCookie12,
    "ghostish":      makeGhostish,
    "clover4leaf":   makeClover4,
    "clover8leaf":   makeClover8,
    "burst":         makeBurst,
    "softburst":     makeSoftBurst,
    "boom":          makeBoom,
    "softboom":      makeSoftBoom,
    "flower":        makeFlower,
    "puffy":         makePuffy,
    "puffydiamond":  makePuffyDiamond,
    "cloud":         makeCloud,
    "pixelcircle":   makePixelCircle,
    "pixeltriangle": makePixelTriangle,
    "bun":           makeBun,
    "heart":         makeHeart
};

var _cache = {};

function canonical(name) {
    return (name || "").toLowerCase().replace(/[\s_\-]/g, "");
}

function get(typeName) {
    var key = canonical(typeName);
    if (_cache[key]) return _cache[key];
    var fn = _builders[key] || _builders["circle"];
    var shape = fn();
    _cache[key] = shape;
    return shape;
}

function names() { return Object.keys(_builders); }
