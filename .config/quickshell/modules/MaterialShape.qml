import QtQuick

// Single floating Material Design shape rendered via Canvas
Item {
    id: root

    property color shapeColor: "#e2b7f4"
    property real shapeOpacity: 0.08
    property int shapeIndex: 0

    property real floatRangeX: 60
    property real floatRangeY: 40
    property real floatDuration: 20000
    property real rotateDuration: 40000

    width: 120
    height: 120

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Threaded

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)
            ctx.fillStyle = root.shapeColor
            ctx.globalAlpha = root.shapeOpacity

            var idx = root.shapeIndex % 18
            drawShape(ctx, idx, w, h)
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        function drawShape(ctx, idx, w, h) {
            ctx.beginPath()
            switch (idx) {
            case 0: circle(ctx, w, h); break
            case 1: roundedSquare(ctx, w, h); break
            case 2: slanted(ctx, w, h); break
            case 3: diamond(ctx, w, h); break
            case 4: triangle(ctx, w, h); break
            case 5: heart(ctx, w, h); break
            case 6: arch(ctx, w, h); break
            case 7: semicircle(ctx, w, h); break
            case 8: pentagon(ctx, w, h); break
            case 9: pill(ctx, w, h); break
            case 10: fan(ctx, w, h); break
            case 11: hexagon(ctx, w, h); break
            case 12: clover4(ctx, w, h); break
            case 13: star(ctx, w, h); break
            case 14: oval(ctx, w, h); break
            case 15: flower(ctx, w, h); break
            case 16: cloud(ctx, w, h); break
            case 17: cookie(ctx, w, h); break
            }
            ctx.fill()
        }

        // 0: Circle
        function circle(ctx, w, h) {
            ctx.arc(w/2, h/2, w*0.48, 0, Math.PI*2)
        }

        // 1: Rounded square
        function roundedSquare(ctx, w, h) {
            var r = w * 0.18
            var m = w * 0.04
            ctx.moveTo(m + r, m)
            ctx.lineTo(w - m - r, m)
            ctx.quadraticCurveTo(w - m, m, w - m, m + r)
            ctx.lineTo(w - m, h - m - r)
            ctx.quadraticCurveTo(w - m, h - m, w - m - r, h - m)
            ctx.lineTo(m + r, h - m)
            ctx.quadraticCurveTo(m, h - m, m, h - m - r)
            ctx.lineTo(m, m + r)
            ctx.quadraticCurveTo(m, m, m + r, m)
        }

        // 2: Slanted blob
        function slanted(ctx, w, h) {
            ctx.moveTo(w*0.3, w*0.02)
            ctx.quadraticCurveTo(w*0.5, -w*0.04, w*0.8, w*0.05)
            ctx.quadraticCurveTo(w*0.98, w*0.1, w*0.95, w*0.35)
            ctx.quadraticCurveTo(w*0.98, w*0.7, w*0.75, w*0.95)
            ctx.quadraticCurveTo(w*0.5, w*1.04, w*0.2, w*0.95)
            ctx.quadraticCurveTo(w*0.02, w*0.9, w*0.05, w*0.6)
            ctx.quadraticCurveTo(w*0.02, w*0.2, w*0.3, w*0.02)
        }

        // 3: Diamond
        function diamond(ctx, w, h) {
            var r = w * 0.08
            ctx.moveTo(w/2, h*0.02)
            ctx.quadraticCurveTo(w/2 + r, h*0.02, w*0.98, h/2 - r)
            ctx.quadraticCurveTo(w*0.98, h/2, w*0.98, h/2 + r)
            ctx.quadraticCurveTo(w*0.98, h/2 + r, w/2 + r, h*0.98)
            ctx.quadraticCurveTo(w/2, h*0.98, w/2 - r, h*0.98)
            ctx.quadraticCurveTo(w*0.02, h/2 + r, w*0.02, h/2)
            ctx.quadraticCurveTo(w*0.02, h/2 - r, w/2 - r, h*0.02)
            ctx.quadraticCurveTo(w/2, h*0.02, w/2, h*0.02)
        }

        // 4: Rounded triangle
        function triangle(ctx, w, h) {
            var r = w * 0.06
            ctx.moveTo(w/2, h*0.05)
            ctx.quadraticCurveTo(w*0.55, h*0.05, w*0.95, h*0.85)
            ctx.quadraticCurveTo(w*0.95, h*0.95, w*0.85, h*0.95)
            ctx.lineTo(w*0.15, h*0.95)
            ctx.quadraticCurveTo(w*0.05, h*0.95, w*0.05, h*0.85)
            ctx.quadraticCurveTo(w*0.45, h*0.05, w/2, h*0.05)
        }

        // 5: Heart
        function heart(ctx, w, h) {
            var cx = w / 2
            var top = h * 0.3
            var bot = h * 0.82
            ctx.moveTo(cx, bot)
            ctx.bezierCurveTo(cx - w*0.45, h*0.6, cx - w*0.45, h*0.15, cx - w*0.22, h*0.15)
            ctx.bezierCurveTo(cx - w*0.08, h*0.15, cx, h*0.25, cx, top)
            ctx.bezierCurveTo(cx, h*0.25, cx + w*0.08, h*0.15, cx + w*0.22, h*0.15)
            ctx.bezierCurveTo(cx + w*0.45, h*0.15, cx + w*0.45, h*0.6, cx, bot)
        }

        // 6: Arch
        function arch(ctx, w, h) {
            ctx.moveTo(w*0.1, h*0.98)
            ctx.lineTo(w*0.1, h*0.4)
            ctx.quadraticCurveTo(w*0.1, h*0.02, w/2, h*0.02)
            ctx.quadraticCurveTo(w*0.9, h*0.02, w*0.9, h*0.4)
            ctx.lineTo(w*0.9, h*0.98)
            ctx.quadraticCurveTo(w/2, h*0.92, w*0.1, h*0.98)
        }

        // 7: Semicircle
        function semicircle(ctx, w, h) {
            ctx.arc(w/2, h*0.55, w*0.48, Math.PI, 0)
            ctx.closePath()
        }

        // 8: Pentagon
        function pentagon(ctx, w, h) {
            var cx = w/2, cy = h/2, r = w*0.46
            for (var i = 0; i < 5; i++) {
                var angle = (i * 2 * Math.PI / 5) - Math.PI/2
                var px = cx + r * Math.cos(angle)
                var py = cy + r * Math.sin(angle)
                if (i === 0) ctx.moveTo(px, py)
                else ctx.lineTo(px, py)
            }
            ctx.closePath()
        }

        // 9: Pill
        function pill(ctx, w, h) {
            var r = h * 0.25
            ctx.moveTo(w*0.3, h*0.25)
            ctx.lineTo(w*0.7, h*0.25)
            ctx.arcTo(w*0.95, h*0.25, w*0.95, h/2, r)
            ctx.arcTo(w*0.95, h*0.75, w*0.7, h*0.75, r)
            ctx.lineTo(w*0.3, h*0.75)
            ctx.arcTo(w*0.05, h*0.75, w*0.05, h/2, r)
            ctx.arcTo(w*0.05, h*0.25, w*0.3, h*0.25, r)
        }

        // 10: Fan
        function fan(ctx, w, h) {
            ctx.moveTo(w/2, h*0.92)
            ctx.quadraticCurveTo(w*0.4, h*0.85, w*0.05, h*0.2)
            ctx.quadraticCurveTo(w*0.05, h*0.05, w*0.2, h*0.05)
            ctx.lineTo(w/2, h*0.25)
            ctx.lineTo(w*0.8, h*0.05)
            ctx.quadraticCurveTo(w*0.95, h*0.05, w*0.95, h*0.2)
            ctx.quadraticCurveTo(w*0.6, h*0.85, w/2, h*0.92)
        }

        // 11: Hexagon
        function hexagon(ctx, w, h) {
            var cx = w/2, cy = h/2, r = w*0.46
            for (var i = 0; i < 6; i++) {
                var angle = (i * Math.PI / 3) - Math.PI/2
                var px = cx + r * Math.cos(angle)
                var py = cy + r * Math.sin(angle)
                if (i === 0) ctx.moveTo(px, py)
                else ctx.lineTo(px, py)
            }
            ctx.closePath()
        }

        // 12: 4-leaf clover
        function clover4(ctx, w, h) {
            var cx = w/2, cy = h/2, lr = w*0.22
            ctx.arc(cx, cy - lr, lr, 0, Math.PI*2)
            ctx.moveTo(cx + lr*2, cy)
            ctx.arc(cx + lr, cy, lr, 0, Math.PI*2)
            ctx.moveTo(cx + lr, cy + lr*2)
            ctx.arc(cx, cy + lr, lr, 0, Math.PI*2)
            ctx.moveTo(cx - lr + lr, cy)
            ctx.arc(cx - lr, cy, lr, 0, Math.PI*2)
        }

        // 13: 5-point star
        function star(ctx, w, h) {
            var cx = w/2, cy = h/2, or_ = w*0.46, ir = w*0.2
            for (var i = 0; i < 10; i++) {
                var r = (i % 2 === 0) ? or_ : ir
                var angle = (i * Math.PI / 5) - Math.PI/2
                var px = cx + r * Math.cos(angle)
                var py = cy + r * Math.sin(angle)
                if (i === 0) ctx.moveTo(px, py)
                else ctx.lineTo(px, py)
            }
            ctx.closePath()
        }

        // 14: Oval
        function oval(ctx, w, h) {
            ctx.ellipse(w*0.08, h*0.2, w*0.84, h*0.6)
        }

        // 15: Flower (6 petals)
        function flower(ctx, w, h) {
            var cx = w/2, cy = h/2, pr = w*0.2
            for (var i = 0; i < 6; i++) {
                var angle = i * Math.PI / 3
                var px = cx + pr * Math.cos(angle)
                var py = cy + pr * Math.sin(angle)
                ctx.moveTo(px + pr*0.8, py)
                ctx.arc(px, py, pr*0.8, 0, Math.PI*2)
            }
        }

        // 16: Cloud / Puffy
        function cloud(ctx, w, h) {
            var cy = h * 0.52
            var base = h * 0.62
            // Bottom flat
            ctx.moveTo(w*0.15, base)
            ctx.lineTo(w*0.85, base)
            // Right bump
            ctx.arc(w*0.72, cy, w*0.16, Math.PI*0.35, -Math.PI*0.8, true)
            // Top bump
            ctx.arc(w*0.48, h*0.32, w*0.22, -Math.PI*0.15, -Math.PI*0.85, true)
            // Left bump
            ctx.arc(w*0.25, cy, w*0.17, -Math.PI*0.3, Math.PI*0.65, true)
            ctx.closePath()
        }

        // 17: Cookie (scalloped circle with flat-bottomed bumps)
        function cookie(ctx, w, h) {
            var cx = w / 2
            var cy = h / 2
            var bumps = 12
            var outerR = w * 0.46
            var innerR = w * 0.38
            var bumpR = (outerR - innerR)

            for (var i = 0; i < bumps; i++) {
                var a1 = (i / bumps) * Math.PI * 2 - Math.PI / 2
                var a2 = ((i + 1) / bumps) * Math.PI * 2 - Math.PI / 2
                var aMid = (a1 + a2) / 2

                var ix1 = cx + innerR * Math.cos(a1)
                var iy1 = cy + innerR * Math.sin(a1)
                var ox = cx + outerR * Math.cos(aMid)
                var oy = cy + outerR * Math.sin(aMid)
                var ix2 = cx + innerR * Math.cos(a2)
                var iy2 = cy + innerR * Math.sin(a2)

                if (i === 0) ctx.moveTo(ix1, iy1)

                ctx.quadraticCurveTo(ox, oy, ix2, iy2)
            }
            ctx.closePath()
        }
    }

    Component.onCompleted: canvas.requestPaint()
    onShapeColorChanged: canvas.requestPaint()
    onShapeOpacityChanged: canvas.requestPaint()

    // Slow floating X
    SequentialAnimation on x {
        loops: Animation.Infinite
        NumberAnimation {
            from: root.x
            to: root.x + root.floatRangeX
            duration: root.floatDuration
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            from: root.x + root.floatRangeX
            to: root.x - root.floatRangeX
            duration: root.floatDuration * 2
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            from: root.x - root.floatRangeX
            to: root.x
            duration: root.floatDuration
            easing.type: Easing.InOutSine
        }
    }

    // Slow floating Y
    SequentialAnimation on y {
        loops: Animation.Infinite
        NumberAnimation {
            from: root.y
            to: root.y - root.floatRangeY
            duration: root.floatDuration * 1.3
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            from: root.y - root.floatRangeY
            to: root.y + root.floatRangeY
            duration: root.floatDuration * 2.6
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            from: root.y + root.floatRangeY
            to: root.y
            duration: root.floatDuration * 1.3
            easing.type: Easing.InOutSine
        }
    }

    // Slow rotation
    RotationAnimation on rotation {
        loops: Animation.Infinite
        from: -15
        to: 15
        duration: root.rotateDuration
        easing.type: Easing.InOutSine
    }
}
