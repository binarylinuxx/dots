import QtQuick 2.15
import "RoundedPolygon.js" as RP
import "Morph.js" as Morph

Canvas {
    id: root

    property int sides: 6
    property real innerRatio: 0.78
    property real outerRounding: 0.40
    property real innerRounding: 0.40
    property real smoothing: 0.0
    property real rotationDegrees: 0

    property color color: "#4285F4"
    property real size: 220
    property real strokeWidth: 0
    property color strokeColor: "transparent"
    property int morphDuration: 350
    property int morphSamples: 64

    width: size
    height: size

    property var _fromShape: null
    property var _toShape: null
    property var _morph: null
    property real _t: 1.0
    property bool _animating: false
    
    // Animated rotation value - separate from target (no underscore prefix for QML signal handler)
    property real displayRotation: 0

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v))
    }

    function buildCookieShape() {
        var n = Math.max(3, Math.round(root.sides))
        var inner = clamp(root.innerRatio, 0.05, 0.98)
        var outerR = clamp(root.outerRounding, 0.0, 1.0)
        var innerR = clamp(root.innerRounding, 0.0, 1.0)
        var sm = clamp(root.smoothing, 0.0, 1.0)

        var pvr = []
        for (var i = 0; i < n * 2; i++) {
            var rr = (i % 2 === 0) ? outerR : innerR
            pvr.push({ radius: rr, smoothing: sm })
        }

        // Build upright shape - rotation applied separately during paint
        var s = RP.buildStarRaw(n, 1.0, inner, pvr, 0, 0)
        return RP.normalizedShape(s)
    }

    function rebuildTarget() {
        var next = buildCookieShape()
        var current = _toShape || next
        _fromShape = current
        _toShape = next
        _morph = Morph.prepareMorph(current, next, morphSamples)
        _t = 0.0
        _animating = true
        _anim.restart()
    }
    
    function animateRotation() {
        _rotAnim.from = root.displayRotation
        _rotAnim.to = root.rotationDegrees
        _rotAnim.restart()
    }

    Component.onCompleted: {
        var s = buildCookieShape()
        _fromShape = s
        _toShape = s
        _morph = null
        _t = 1.0
        displayRotation = rotationDegrees
        requestPaint()
    }

    // Shape properties trigger morph rebuild
    onSidesChanged: rebuildTarget()
    onInnerRatioChanged: rebuildTarget()
    onOuterRoundingChanged: rebuildTarget()
    onInnerRoundingChanged: rebuildTarget()
    onSmoothingChanged: rebuildTarget()
    onMorphSamplesChanged: rebuildTarget()
    
    // Rotation triggers separate animation
    onRotationDegreesChanged: animateRotation()

    // Morph animation for shape changes
    NumberAnimation on _t {
        id: _anim
        from: 0.0
        to: 1.0
        duration: root.morphDuration
        easing.type: Easing.InOutQuad
        onRunningChanged: {
            if (!running) {
                root._animating = false
                root.requestPaint()
            }
        }
    }
    
    // Rotation animation - runs independently
    NumberAnimation on displayRotation {
        id: _rotAnim
        duration: root.morphDuration
        easing.type: Easing.InOutQuad
        onRunningChanged: {
            if (!running) {
                root.requestPaint()
            }
        }
    }

    on_TChanged: requestPaint()
    onDisplayRotationChanged: requestPaint()
    onColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onSizeChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

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
        
        // Apply rotation around center of normalized shape
        ctx.translate(0.5, 0.5)
        ctx.rotate(root.displayRotation * Math.PI / 180)
        ctx.translate(-0.5, -0.5)
        
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
            ctx.lineWidth = root.strokeWidth / s
            ctx.stroke()
        }
        ctx.restore()
    }
}
