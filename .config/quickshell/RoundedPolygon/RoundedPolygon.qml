import QtQuick 2.15
import "MaterialShapes.js" as MS
import "Morph.js" as Morph

// RoundedPolygon
// Renders any registered rounded-polygon shape (including custom presets).
// Uses Canvas 2D bezierCurveTo for pixel-perfect cubic Bézier rendering.
//
// API:
//   type            string    shape name (see MaterialShapes.js)
//   color           color     fill colour
//   size            real      width == height (square canvas)
//   strokeWidth     real      outline width (0 = no stroke)
//   strokeColor     color     outline colour
//   morphDuration   int       ms for the shape-change animation
//   morphSamples    int       cubic count for morph resampling (higher = smoother)

Canvas {
    id: root

    property string type:         "circle"
    property color  color:        "#4285F4"
    property real   size:         220
    property real   strokeWidth:  0
    property color  strokeColor:  "transparent"
    property int    morphDuration: 400
    property int    morphSamples:  64

    width:  size
    height: size

    // ── internal morph state ─────────────────────────────────────────────────

    property var  _fromShape: null
    property var  _toShape:   null
    property var  _morph:     null
    property real _t:         1.0
    property bool _animating: false

    // ── initialise ───────────────────────────────────────────────────────────

    Component.onCompleted: {
        var s = MS.get(type)
        _fromShape = s
        _toShape   = s
        _morph     = null
        _t         = 1.0
        requestPaint()
    }

    onTypeChanged: {
        var next    = MS.get(type)
        var current = _toShape || next
        _fromShape  = current
        _toShape    = next
        _morph      = Morph.prepareMorph(current, next, morphSamples)
        _t          = 0.0
        _animating  = true
        _anim.restart()
    }

    NumberAnimation on _t {
        id:          _anim
        from:        0.0
        to:          1.0
        duration:    root.morphDuration
        easing.type: Easing.InOutQuad
        onRunningChanged: {
            if (!running) {
                root._animating = false
                root.requestPaint()
            }
        }
    }

    on_TChanged:          requestPaint()
    onColorChanged:       requestPaint()
    onStrokeWidthChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onSizeChanged:        requestPaint()

    // ── paint ────────────────────────────────────────────────────────────────

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        // Static: use original cubics directly — no resampling distortion
        // Animating: use morph-resampled interpolation
        var cubics
        if (!_animating && _toShape) {
            cubics = _toShape.cubics
        } else if (_morph) {
            cubics = Morph.interpolateCubics(_morph.from, _morph.to, _t)
        } else if (_toShape) {
            cubics = _toShape.cubics
        } else {
            return
        }
        if (!cubics || cubics.length === 0) return

        var s = Math.min(width, height)

        ctx.save()
        ctx.scale(s, s)

        ctx.beginPath()
        ctx.moveTo(cubics[0][0], cubics[0][1])
        for (var i = 0; i < cubics.length; i++) {
            var c = cubics[i]
            ctx.bezierCurveTo(c[2], c[3], c[4], c[5], c[6], c[7])
        }
        ctx.closePath()

        ctx.fillStyle = root.color
        ctx.fill()

        if (root.strokeWidth > 0) {
            ctx.strokeStyle = root.strokeColor
            ctx.lineWidth   = root.strokeWidth / s
            ctx.stroke()
        }

        ctx.restore()
    }
}
