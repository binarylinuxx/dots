.pragma library

// RoundedPolygon.js
// Converts a vertex list + per-vertex CornerRounding into cubic Bézier segments.
//
// Algorithm mirrors androidx.graphics.shapes (Kotlin / reference JS port):
//   • each corner produces up to 3 cubics: flanking0 → circularArc → flanking2(reversed)
//   • flanking curves use the correct anchorEnd = line–tangent intersection
//   • adjacent-corner cut clamping prevents overlaps
//
// Public API
//   buildShape(vertices, perVertexRounding)  →  { cubics }
//   normalizedShape(shape)                   →  shape scaled to [0,1]²
//   transformedShape(shape, fn)              →  fn(x,y)→{x,y} on every point
//   circle(numVerts, radius, cx, cy)         →  shape
//   rectangle(w, h, pvr, cx, cy)            →  shape
//   star(numV, outerR, innerR, pvr, cx, cy) →  shape
//   rotated(shape, degrees)                  →  shape
//   scaled(shape, sx, sy)                    →  shape
//
// CornerRounding  { radius, smoothing }   smoothing ∈ [0,1]
// Cubic           flat Array[8]           [a0x,a0y, c0x,c0y, c1x,c1y, a1x,a1y]

var EPS = 1e-4;

// ─── math helpers ────────────────────────────────────────────────────────────

function lerp(a, b, t) { return a + (b - a) * t; }

function vlen(dx, dy) { return Math.sqrt(dx * dx + dy * dy); }

function vdir(dx, dy) {
    var l = vlen(dx, dy);
    return l > EPS ? [dx / l, dy / l] : [0, 0];
}

function vdot(ax, ay, bx, by) { return ax * bx + ay * by; }

function vdist(ax, ay, bx, by) { return vlen(bx - ax, by - ay); }

// ─── Cubic helpers ───────────────────────────────────────────────────────────

function makeCubic(a0x, a0y, c0x, c0y, c1x, c1y, a1x, a1y) {
    return [a0x, a0y, c0x, c0y, c1x, c1y, a1x, a1y];
}

function lineCubic(x0, y0, x1, y1) {
    return makeCubic(x0, y0,
                     lerp(x0, x1, 1/3), lerp(y0, y1, 1/3),
                     lerp(x0, x1, 2/3), lerp(y0, y1, 2/3),
                     x1, y1);
}

function zeroCubic(x, y) {
    return makeCubic(x, y, x, y, x, y, x, y);
}

function zeroLength(c) {
    return Math.abs(c[0] - c[6]) < EPS && Math.abs(c[1] - c[7]) < EPS;
}

function reverseCubic(c) {
    return [c[6], c[7], c[4], c[5], c[2], c[3], c[0], c[1]];
}

// Best single-cubic approximation of a circular arc from (x0,y0) to (x1,y1)
// on a circle centred at (cx,cy).
// Uses the standard k = (4/3)*tan(α/4) formula with normalised tangents.
function arcCubic(cx, cy, x0, y0, x1, y1) {
    var d0x = x0 - cx, d0y = y0 - cy;
    var d1x = x1 - cx, d1y = y1 - cy;
    var R = vlen(d0x, d0y);
    if (R < EPS) return lineCubic(x0, y0, x1, y1);

    // cosine of the arc angle
    var cosa = vdot(d0x, d0y, d1x, d1y) / (R * vlen(d1x, d1y) + EPS);
    cosa = Math.max(-1, Math.min(1, cosa));
    if (cosa > 1 - EPS) return lineCubic(x0, y0, x1, y1);

    // unit tangents (CCW rotation of unit-radius direction)
    var t0x = -d0y / R, t0y = d0x / R;
    var t1x = -d1y / R, t1y = d1x / R;

    // Are we going clockwise?  CCW tangent at P0 should point towards P1 if CW.
    var cw   = t0x * d1x + t0y * d1y >= 0;
    var sign = cw ? 1 : -1;

    // k = R * (4/3) * tan(α/4)  expressed in terms of cos(α)
    var k = R * (4.0 / 3.0) *
            (Math.sqrt(2 * (1 - cosa)) - Math.sqrt(1 - cosa * cosa)) /
            (1 - cosa) * sign;

    return makeCubic(
        x0,              y0,
        x0 + t0x * k,   y0 + t0y * k,
        x1 - t1x * k,   y1 - t1y * k,
        x1,              y1
    );
}

// ─── Per-corner rounding ─────────────────────────────────────────────────────
//
// Replicates RoundedCorner.getCubics + computeFlankingCurve from the reference.
//
//  p0 → p1 → p2
//  rounding = { radius, smoothing }
//  allowedCut0 = max cut the previous edge can give this corner
//  allowedCut1 = max cut the next edge can give this corner
//
// Returns Array of 1 or 3 cubics.

function roundCorner(p0x, p0y, p1x, p1y, p2x, p2y, rounding, allowedCut0, allowedCut1) {
    var radius = (rounding && rounding.radius)   ? rounding.radius   : 0;
    var smooth = (rounding && rounding.smoothing)? rounding.smoothing: 0;

    if (radius < EPS || allowedCut0 < EPS || allowedCut1 < EPS)
        return [zeroCubic(p1x, p1y)];

    // unit vectors from corner towards each neighbour
    var d1 = vdir(p0x - p1x, p0y - p1y);   // towards p0 (incoming edge)
    var d2 = vdir(p2x - p1x, p2y - p1y);   // towards p2 (outgoing edge)

    var cosA = Math.max(-1, Math.min(1, vdot(d1[0], d1[1], d2[0], d2[1])));
    var sinA = Math.sqrt(Math.max(0, 1 - cosA * cosA));
    if (sinA < EPS) return [zeroCubic(p1x, p1y)];

    // how far along each edge the rounding circle is tangent
    var expRoundCut = radius * (cosA + 1) / sinA;
    if (expRoundCut < EPS) return [zeroCubic(p1x, p1y)];

    var expCut     = (1 + smooth) * expRoundCut;
    var allowedCut = Math.min(allowedCut0, allowedCut1);
    if (allowedCut < EPS) return [zeroCubic(p1x, p1y)];

    var actualRoundCut = Math.min(allowedCut, expRoundCut);
    var actualR        = radius * actualRoundCut / expRoundCut;

    // per-side smoothing, scaled by available space
    function calcSmooth(ac) {
        if (ac >= expCut)        return smooth;
        if (ac > expRoundCut)    return smooth * (ac - expRoundCut) / (expCut - expRoundCut);
        return 0;
    }
    var sm0 = calcSmooth(allowedCut0);
    var sm1 = calcSmooth(allowedCut1);

    // centre of rounding circle: along bisector at distance sqrt(R²+cut²)
    var bisDir    = vdir(d1[0] + d2[0], d1[1] + d2[1]);
    var centerDist= Math.sqrt(actualR * actualR + actualRoundCut * actualRoundCut);
    var ccx = p1x + bisDir[0] * centerDist;
    var ccy = p1y + bisDir[1] * centerDist;

    // tangent points: where the circle meets each edge
    var ci0x = p1x + d1[0] * actualRoundCut,  ci0y = p1y + d1[1] * actualRoundCut;
    var ci2x = p1x + d2[0] * actualRoundCut,  ci2y = p1y + d2[1] * actualRoundCut;

    // ── Flanking curve ──────────────────────────────────────────────────────
    // Computes the cubic that blends the straight edge into the circular arc.
    //
    //   smVal      = actualSmoothing for this side
    //   sideDir    = direction from p1 towards sideStart (d1 or d2)
    //   sideStartX/Y = far vertex on this edge (p0 or p2)
    //   tiAx/y     = circle tangent point on THIS edge
    //   tiBx/y     = circle tangent point on the OTHER edge
    //
    // Mirrors computeFlankingCurve in rounded-corner.js exactly.

    function flankCurve(smVal, sideDir, sideStartX, sideStartY,
                        tiAx, tiAy, tiBx, tiBy) {

        // curveStart: walks out along the edge, further when smoothing > 0
        var csX = p1x + sideDir[0] * actualRoundCut * (1 + smVal);
        var csY = p1y + sideDir[1] * actualRoundCut * (1 + smVal);

        // curveEnd: where the flanking cubic lands on the circle
        // interpolate circle tangent point towards arc midpoint by smVal
        var midX = (tiAx + tiBx) * 0.5,  midY = (tiAy + tiBy) * 0.5;
        var pX   = lerp(tiAx, midX, smVal);
        var pY   = lerp(tiAy, midY, smVal);
        var pd   = vdir(pX - ccx, pY - ccy);
        var ceX  = ccx + pd[0] * actualR;
        var ceY  = ccy + pd[1] * actualR;

        // circle tangent at curveEnd: rotate pd by 90°
        var ctX = -pd[1],  ctY = pd[0];

        // anchorEnd = intersection of line(sideStart + t*sideDir)
        //                       with line(curveEnd + s*circleTangent)
        // Solve via: rotate circleTangent 90° → rotCT = (-ctY, ctX) = (pd[0], pd[1]) = -pd
        // Actually: rotate90(ct) = rotate90(-pd[1], pd[0]) = (-pd[0], -pd[1])
        // Reference: den = sideDir · rotatedD1   where rotatedD1 = rotate90(circleTangent)
        var rcX = -ctY,  rcY = ctX;           // rotate90(circleTangent)
        var den = sideDir[0] * rcX + sideDir[1] * rcY;
        var aeX, aeY;
        if (Math.abs(den) > EPS) {
            var dX = ceX - sideStartX,  dY = ceY - sideStartY;
            var t  = (dX * rcX + dY * rcY) / den;
            aeX = sideStartX + sideDir[0] * t;
            aeY = sideStartY + sideDir[1] * t;
        } else {
            aeX = tiAx;  aeY = tiAy;
        }

        // anchorStart = (curveStart + 2*anchorEnd) / 3
        var asX = (csX + 2 * aeX) / 3;
        var asY = (csY + 2 * aeY) / 3;

        return [csX, csY, asX, asY, aeX, aeY, ceX, ceY];
    }

    var f0    = flankCurve(sm0, d1, p0x, p0y, ci0x, ci0y, ci2x, ci2y);
    var f2raw = flankCurve(sm1, d2, p2x, p2y, ci2x, ci2y, ci0x, ci0y);
    var f2    = reverseCubic(f2raw);

    var arc = arcCubic(ccx, ccy, f0[6], f0[7], f2[0], f2[1]);

    return [f0, arc, f2];
}

// ─── Shape builder ───────────────────────────────────────────────────────────
//
// vertices         flat [x0,y0, x1,y1, …]
// perVertexRounding  Array<{ radius, smoothing }> or null

function buildShape(vertices, perVertexRounding) {
    var n = vertices.length / 2;
    var noRound = { radius: 0, smoothing: 0 };

    var roundings = [];
    for (var i = 0; i < n; i++)
        roundings.push(perVertexRounding ? (perVertexRounding[i] || noRound) : noRound);

    // expected cuts per corner (before clamping)
    function expRC(i) {
        var ro = roundings[i];
        if (!ro || ro.radius < EPS) return 0;
        var p0i = ((i - 1 + n) % n) * 2;
        var p1i = i * 2;
        var p2i = ((i + 1) % n) * 2;
        var d1 = vdir(vertices[p0i]   - vertices[p1i],   vertices[p0i+1] - vertices[p1i+1]);
        var d2 = vdir(vertices[p2i]   - vertices[p1i],   vertices[p2i+1] - vertices[p1i+1]);
        var cosA = Math.max(-1, Math.min(1, vdot(d1[0], d1[1], d2[0], d2[1])));
        var sinA = Math.sqrt(Math.max(0, 1 - cosA * cosA));
        if (sinA < EPS) return 0;
        return ro.radius * (cosA + 1) / sinA;
    }

    function expC(i) {
        var ro = roundings[i];
        var sm = (ro && ro.smoothing) ? ro.smoothing : 0;
        return (1 + sm) * expRC(i);
    }

    // per-side allowed-cut ratios, clamping corners that would overlap
    var sideRatios = [];
    for (var s = 0; s < n; s++) {
        var ni = (s + 1) % n;
        var si = s * 2, nii = ni * 2;
        var sideLen = vdist(vertices[si], vertices[si+1], vertices[nii], vertices[nii+1]);
        var erc0 = expRC(s),  erc1 = expRC(ni);
        var ec0  = expC(s),   ec1  = expC(ni);
        var totalRC = erc0 + erc1, totalC = ec0 + ec1;
        var ratioRC, ratioC;
        if (totalRC > sideLen + EPS) {
            ratioRC = sideLen / totalRC; ratioC = 0;
        } else if (totalC > sideLen + EPS) {
            ratioRC = 1;
            ratioC  = (sideLen - totalRC) / (totalC - totalRC + EPS);
        } else {
            ratioRC = 1; ratioC = 1;
        }
        sideRatios.push({ rc: ratioRC, c: ratioC });
    }

    function allowedCut(cornerIdx, sideIdx) {
        var sr  = sideRatios[sideIdx];
        var erc = expRC(cornerIdx);
        var ec  = expC(cornerIdx);
        return erc * sr.rc + (ec - erc) * sr.c;
    }

    // build per-corner cubics
    var cornerCubics = [];
    for (var ci = 0; ci < n; ci++) {
        var prev = ((ci - 1 + n) % n) * 2;
        var curr = ci * 2;
        var next = ((ci + 1) % n) * 2;
        var ac0  = allowedCut(ci, (ci - 1 + n) % n);  // incoming side
        var ac1  = allowedCut(ci, ci);                  // outgoing side
        cornerCubics.push(roundCorner(
            vertices[prev], vertices[prev+1],
            vertices[curr], vertices[curr+1],
            vertices[next], vertices[next+1],
            roundings[ci], ac0, ac1
        ));
    }

    // assemble: corner cubics + straight edges
    var cubics = [];
    for (var j = 0; j < n; j++) {
        var cc = cornerCubics[j];
        for (var k = 0; k < cc.length; k++)
            if (!zeroLength(cc[k])) cubics.push(cc[k]);

        // straight edge to start of next corner
        var lastC   = cc[cc.length - 1];
        var nextCC  = cornerCubics[(j + 1) % n];
        var ex0 = lastC[6],    ey0 = lastC[7];
        var ex1 = nextCC[0][0], ey1 = nextCC[0][1];
        if (vdist(ex0, ey0, ex1, ey1) > EPS)
            cubics.push(lineCubic(ex0, ey0, ex1, ey1));
    }

    return { cubics: cubics };
}

// ─── normalization / transform ────────────────────────────────────────────────

function cubicBounds(c) {
    return [
        Math.min(c[0], c[2], c[4], c[6]),
        Math.min(c[1], c[3], c[5], c[7]),
        Math.max(c[0], c[2], c[4], c[6]),
        Math.max(c[1], c[3], c[5], c[7])
    ];
}

function shapeBounds(shape) {
    var mnX = Infinity, mnY = Infinity, mxX = -Infinity, mxY = -Infinity;
    var cs = shape.cubics;
    for (var i = 0; i < cs.length; i++) {
        var b = cubicBounds(cs[i]);
        if (b[0] < mnX) mnX = b[0];
        if (b[1] < mnY) mnY = b[1];
        if (b[2] > mxX) mxX = b[2];
        if (b[3] > mxY) mxY = b[3];
    }
    return [mnX, mnY, mxX, mxY];
}

function transformedShape(shape, fn) {
    var cs = shape.cubics, out = [];
    for (var i = 0; i < cs.length; i++) {
        var c  = cs[i];
        var a0 = fn(c[0], c[1]), k0 = fn(c[2], c[3]);
        var k1 = fn(c[4], c[5]), a1 = fn(c[6], c[7]);
        out.push(makeCubic(a0.x, a0.y, k0.x, k0.y, k1.x, k1.y, a1.x, a1.y));
    }
    return { cubics: out };
}

function normalizedShape(shape) {
    var b    = shapeBounds(shape);
    var w    = b[2] - b[0],  h = b[3] - b[1];
    var side = Math.max(w, h);
    if (side < EPS) return shape;
    var ox = (side - w) / 2 - b[0];
    var oy = (side - h) / 2 - b[1];
    return transformedShape(shape, function(x, y) {
        return { x: (x + ox) / side, y: (y + oy) / side };
    });
}

// ─── raw primitives (un-normalized, for use before Matrix transforms) ─────────

function buildCircleRaw(numVerts, radius, cx, cy) {
    numVerts = numVerts || 8;
    radius   = radius   || 1;
    cx = cx || 0;  cy = cy || 0;
    var theta = Math.PI / numVerts;
    var polyR = radius / Math.cos(theta);
    var verts = [], pvr = [];
    for (var i = 0; i < numVerts; i++) {
        var t = Math.PI / numVerts * 2 * i;
        verts.push(cx + Math.cos(t) * polyR, cy + Math.sin(t) * polyR);
        pvr.push({ radius: radius, smoothing: 0 });
    }
    return buildShape(verts, pvr);
}

function buildRectRaw(w, h, pvr, cx, cy) {
    cx = cx || 0;  cy = cy || 0;
    var hw = w / 2, hh = h / 2;
    return buildShape([
        cx + hw, cy + hh,
        cx - hw, cy + hh,
        cx - hw, cy - hh,
        cx + hw, cy - hh
    ], pvr || null);
}

function buildStarRaw(numV, outerR, innerR, pvr, cx, cy) {
    cx = cx || 0;  cy = cy || 0;
    outerR = outerR || 1;  innerR = innerR || 0.5;
    var verts = [];
    for (var i = 0; i < numV; i++) {
        var to = Math.PI / numV * 2 * i;
        var ti = Math.PI / numV * (2 * i + 1);
        verts.push(cx + Math.cos(to) * outerR, cy + Math.sin(to) * outerR);
        verts.push(cx + Math.cos(ti) * innerR, cy + Math.sin(ti) * innerR);
    }
    return buildShape(verts, pvr || null);
}

function buildRegularPolygonRaw(n, radius, cx, cy, pvr) {
    cx = cx || 0;  cy = cy || 0;  radius = radius || 1;
    var verts = [];
    for (var i = 0; i < n; i++) {
        var t = Math.PI / n * 2 * i;
        verts.push(cx + Math.cos(t) * radius, cy + Math.sin(t) * radius);
    }
    return buildShape(verts, pvr || null);
}

// ─── normalized convenience wrappers ─────────────────────────────────────────

function circle(numVerts, radius, cx, cy) {
    return normalizedShape(buildCircleRaw(numVerts, radius, cx, cy));
}

function rectangle(w, h, pvr, cx, cy) {
    return normalizedShape(buildRectRaw(w, h, pvr, cx, cy));
}

function star(numV, outerR, innerR, pvr, cx, cy) {
    return normalizedShape(buildStarRaw(numV, outerR, innerR, pvr, cx, cy));
}

function rotated(shape, degrees) {
    var t = degrees * Math.PI / 180;
    var co = Math.cos(t), si = Math.sin(t);
    return transformedShape(shape, function(x, y) {
        return { x: x * co - y * si, y: x * si + y * co };
    });
}

function scaled(shape, sx, sy) {
    return transformedShape(shape, function(x, y) {
        return { x: x * sx, y: y * sy };
    });
}
