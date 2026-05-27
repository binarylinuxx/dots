.pragma library

// Morph.js
// Resamples two shapes to equal cubic counts by arc length, then interpolates.
// Only used during animated transitions — static shapes bypass this entirely
// and draw their original cubics directly (see RoundedPolygon.qml).

var EPS = 1e-6;

// ── cubic evaluation ──────────────────────────────────────────────────────────

function cubicPoint(c, t) {
    var u = 1 - t;
    return [
        u*u*u*c[0] + 3*u*u*t*c[2] + 3*u*t*t*c[4] + t*t*t*c[6],
        u*u*u*c[1] + 3*u*u*t*c[3] + 3*u*t*t*c[5] + t*t*t*c[7]
    ];
}

// Cubic length by 8-segment polygonal approximation
function cubicLength(c) {
    var segs = 8, prev = [c[0], c[1]], total = 0;
    for (var i = 1; i <= segs; i++) {
        var p  = cubicPoint(c, i / segs);
        var dx = p[0] - prev[0], dy = p[1] - prev[1];
        total += Math.sqrt(dx * dx + dy * dy);
        prev = p;
    }
    return total;
}

// de Casteljau split at t
function splitAt(c, t) {
    var u  = 1 - t;
    var m0x = u*c[0]+t*c[2], m0y = u*c[1]+t*c[3];
    var m1x = u*c[2]+t*c[4], m1y = u*c[3]+t*c[5];
    var m2x = u*c[4]+t*c[6], m2y = u*c[5]+t*c[7];
    var n0x = u*m0x+t*m1x,   n0y = u*m0y+t*m1y;
    var n1x = u*m1x+t*m2x,   n1y = u*m1y+t*m2y;
    var px  = u*n0x+t*n1x,   py  = u*n0y+t*n1y;
    return {
        a: [c[0],c[1], m0x,m0y, n0x,n0y, px,py],
        b: [px,py,     n1x,n1y, m2x,m2y, c[6],c[7]]
    };
}

// Find parameter t on cubic where arc distance from start equals d
function tAtDist(c, d, totalLen) {
    if (d <= 0)         return 0;
    if (d >= totalLen)  return 1;
    var segs = 8, prev = [c[0], c[1]], walked = 0;
    for (var i = 1; i <= segs; i++) {
        var ti = i / segs;
        var p  = cubicPoint(c, ti);
        var dx = p[0]-prev[0], dy = p[1]-prev[1];
        var sl = Math.sqrt(dx*dx + dy*dy);
        if (walked + sl >= d - EPS) {
            var frac = sl > EPS ? (d - walked) / sl : 0;
            return (i - 1) / segs + frac / segs;
        }
        walked += sl;
        prev = p;
    }
    return 1;
}

// ── resample ──────────────────────────────────────────────────────────────────
// Redistribute the cubics of a shape into `count` equal-arc-length cubics.
// Uses proper de Casteljau extraction — no straight-line approximations.

function resampleShape(shape, count) {
    var cs    = shape.cubics;
    var ncs   = cs.length;
    var lens  = [];
    for (var i = 0; i < ncs; i++) lens.push(cubicLength(cs[i]));
    var total = 0;
    for (var j = 0; j < lens.length; j++) total += lens[j];

    if (total < EPS) {
        var x0 = cs[0][0], y0 = cs[0][1], out0 = [];
        for (var z = 0; z < count; z++)
            out0.push([x0,y0,x0,y0,x0,y0,x0,y0]);
        return out0;
    }

    // Prefix sums for fast cubic-index lookup
    var prefix = [0];
    for (var k = 0; k < ncs; k++) prefix.push(prefix[k] + lens[k]);

    var step = total / count;
    var out  = [];

    for (var s = 0; s < count; s++) {
        var dStart = s * step;
        var dEnd   = (s + 1) * step;

        // Find which input cubic and t-parameter each endpoint falls on
        function findCubicT(d) {
            if (d <= 0) return { ci: 0, t: 0 };
            if (d >= total) return { ci: ncs - 1, t: 1 };
            var idx = 0;
            while (idx < ncs - 1 && prefix[idx + 1] <= d + EPS) idx++;
            var localD = d - prefix[idx];
            return { ci: idx, t: tAtDist(cs[idx], localD, lens[idx]) };
        }

        var startInfo = findCubicT(dStart);
        var endInfo   = findCubicT(dEnd);

        var ci0 = startInfo.ci, t0 = startInfo.t;
        var ci1 = endInfo.ci,   t1 = endInfo.t;

        var seg;
        if (ci0 === ci1) {
            // Both ends in the same source cubic — exact de Casteljau extraction
            var c = cs[ci0];
            // Extract [t0, t1] sub-curve:
            // 1. split at t0 → keep right half
            var right = t0 > EPS ? splitAt(c, t0).b : c;
            // 2. re-parameterize t1 into the right half
            var tRel = (t0 < 1 - EPS) ? (t1 - t0) / (1 - t0) : 1;
            tRel = Math.max(0, Math.min(1, tRel));
            seg = splitAt(right, tRel).a;
        } else {
            // Spans multiple source cubics.
            // Take tail of ci0 (from t0 to 1) — a real cubic
            var tail = t0 > EPS ? splitAt(cs[ci0], t0).b : cs[ci0];
            // Take head of ci1 (from 0 to t1) — a real cubic
            var head = t1 < 1 - EPS ? splitAt(cs[ci1], t1).a : cs[ci1];
            // Blend: use the cubic that contributes more arc length
            var tailLen = (1 - t0) * lens[ci0];
            var headLen =  t1      * lens[ci1];
            if (tailLen >= headLen) {
                seg = tail;
            } else {
                seg = head;
            }
            // Snap anchor0 to exact start point and anchor1 to exact end point
            var pS = cubicPoint(cs[ci0], t0);
            var pE = cubicPoint(cs[ci1], t1);
            seg = [pS[0],pS[1], seg[2],seg[3], seg[4],seg[5], pE[0],pE[1]];
        }

        out.push(seg);
    }
    return out;
}

// ── public API ────────────────────────────────────────────────────────────────

function prepareMorph(shapeA, shapeB, samples) {
    var n = Math.max(Math.max(shapeA.cubics.length, shapeB.cubics.length), samples || 64);
    // Round up to a multiple of both cubic counts for cleaner alignment
    return {
        from: resampleShape(shapeA, n),
        to:   resampleShape(shapeB, n)
    };
}

function interpolateCubics(from, to, t) {
    var n = Math.min(from.length, to.length), out = [];
    for (var i = 0; i < n; i++) {
        var a = from[i], b = to[i];
        out.push([
            a[0]+(b[0]-a[0])*t, a[1]+(b[1]-a[1])*t,
            a[2]+(b[2]-a[2])*t, a[3]+(b[3]-a[3])*t,
            a[4]+(b[4]-a[4])*t, a[5]+(b[5]-a[5])*t,
            a[6]+(b[6]-a[6])*t, a[7]+(b[7]-a[7])*t
        ]);
    }
    return out;
}
