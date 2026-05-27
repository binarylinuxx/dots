.pragma library

// Matrix.js
// 4×4 column-major matrix matching the reference graphics/matrix.js exactly.
// Rotates around (0,0) — apply to shapes BEFORE normalizing.

function Matrix() {
    // identity
    this.values = [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1];
}

Matrix.prototype.get = function(row, col) {
    return this.values[row * 4 + col];
};

Matrix.prototype.set = function(row, col, v) {
    this.values[row * 4 + col] = v;
};

// Apply this matrix to a point {x, y}, return {x, y}
Matrix.prototype.map = function(x, y) {
    var v00 = this.get(0,0), v01 = this.get(0,1), v03 = this.get(0,3);
    var v10 = this.get(1,0), v11 = this.get(1,1), v13 = this.get(1,3);
    var v30 = this.get(3,0), v31 = this.get(3,1), v33 = this.get(3,3);
    var z   = v03 * x + v13 * y + v33;
    var pZ  = (isFinite(1/z) && z !== 0) ? (1/z) : 0;
    return { x: pZ * (v00 * x + v10 * y + v30),
             y: pZ * (v01 * x + v11 * y + v31) };
};

// rotateZ by degrees around (0,0) — mutates this matrix
Matrix.prototype.rotateZ = function(degrees) {
    var rad = degrees * Math.PI / 180.0;
    var s   = Math.sin(rad), c = Math.cos(rad);
    var a00 = this.get(0,0), a10 = this.get(1,0);
    var a01 = this.get(0,1), a11 = this.get(1,1);
    var a02 = this.get(0,2), a12 = this.get(1,2);
    var a03 = this.get(0,3), a13 = this.get(1,3);
    this.set(0,0,  c*a00 + s*a10);  this.set(1,0, -s*a00 + c*a10);
    this.set(0,1,  c*a01 + s*a11);  this.set(1,1, -s*a01 + c*a11);
    this.set(0,2,  c*a02 + s*a12);  this.set(1,2, -s*a02 + c*a12);
    this.set(0,3,  c*a03 + s*a13);  this.set(1,3, -s*a03 + c*a13);
};

// scale — mutates this matrix
Matrix.prototype.scale = function(sx, sy) {
    sx = (sx === undefined) ? 1 : sx;
    sy = (sy === undefined) ? 1 : sy;
    this.set(0,0, this.get(0,0)*sx); this.set(0,1, this.get(0,1)*sx);
    this.set(0,2, this.get(0,2)*sx); this.set(0,3, this.get(0,3)*sx);
    this.set(1,0, this.get(1,0)*sy); this.set(1,1, this.get(1,1)*sy);
    this.set(1,2, this.get(1,2)*sy); this.set(1,3, this.get(1,3)*sy);
};

// Return a new Matrix with rotateZ applied
function makeRotateZ(degrees) {
    var m = new Matrix();
    m.rotateZ(degrees);
    return m;
}

// Return a new Matrix with scale applied
function makeScale(sx, sy) {
    var m = new Matrix();
    m.scale(sx, sy);
    return m;
}

// Apply a matrix transform to a { cubics } shape
function applyToShape(shape, matrix) {
    var cs  = shape.cubics;
    var out = [];
    for (var i = 0; i < cs.length; i++) {
        var c  = cs[i];
        var a0 = matrix.map(c[0], c[1]);
        var k0 = matrix.map(c[2], c[3]);
        var k1 = matrix.map(c[4], c[5]);
        var a1 = matrix.map(c[6], c[7]);
        out.push([a0.x,a0.y, k0.x,k0.y, k1.x,k1.y, a1.x,a1.y]);
    }
    return { cubics: out };
}
