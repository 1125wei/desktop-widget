import QtQuick

// 卡通形象。
//
// source:
//   "image"  → 用户提供的猫（assets/mascot.png，由 tools/prepare_mascot.py 把白底泛洪抠成透明）
//   "vector" → 自绘矢量猫（Canvas，见文件末尾）—— 用户嫌自绘版不像，所以默认用图片版，
//              矢量版保留着当备选（没有图片素材时也不会开天窗）
Item {
    id: mascot

    // "image" | "vector"
    property string source: "image"
    property url imageSource: "qrc:/assets/mascot.png"

    readonly property bool useImage: source !== "vector"

    // ---------------- 图片版（默认）----------------
    Image {
        anchors.fill: parent
        visible: mascot.useImage
        source: mascot.useImage ? mascot.imageSource : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        // 卡片是 150% DPR 的缩放着色，开 mipmap + smooth 避免缩小后锯齿
        sourceSize.width: Math.round(width * 2)
    }

    // ---------------- 矢量版（备选）----------------
    // 设计稿 120x120，按当前尺寸重绘 → 任意尺寸都清晰
    readonly property real design: 120
    property color outlineColor: "#6E4B33"
    property color furColor: "#D9BC9C"
    property color bodyColor: "#FFFFFF"
    property color markColor: "#3D5FA8"

    Canvas {
        anchors.fill: parent
        visible: !mascot.useImage
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        // 从"图片模式"切回矢量模式时尺寸没变 → 必须手动重绘，否则是空白
        onVisibleChanged: if (visible)
            requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            if (!visible || width <= 0 || height <= 0)
                return
            var s = Math.min(width / mascot.design, height / mascot.design)
            ctx.translate((width - mascot.design * s) / 2, (height - mascot.design * s) / 2)
            ctx.scale(s, s)
            ctx.lineJoin = "round"
            ctx.lineCap = "round"

            var OUT = mascot.outlineColor
            var FUR = mascot.furColor
            var WHT = mascot.bodyColor
            var BLU = mascot.markColor

            // 1) 尾巴
            ctx.strokeStyle = OUT
            ctx.lineWidth = 5
            ctx.beginPath()
            ctx.moveTo(44, 92)
            ctx.bezierCurveTo(34, 96, 26, 103, 12, 99)
            ctx.stroke()
            ctx.strokeStyle = FUR
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(44, 92)
            ctx.bezierCurveTo(34, 96, 26, 103, 12, 99)
            ctx.stroke()

            // 2) 身体
            ctx.beginPath()
            ctx.moveTo(37, 40)
            ctx.bezierCurveTo(28, 56, 26, 78, 34, 90)
            ctx.bezierCurveTo(38, 98, 44, 103, 50, 103)
            ctx.bezierCurveTo(56, 103, 62, 98, 66, 90)
            ctx.bezierCurveTo(74, 78, 72, 56, 63, 40)
            ctx.closePath()
            ctx.fillStyle = WHT
            ctx.fill()
            ctx.strokeStyle = OUT
            ctx.lineWidth = 4
            ctx.stroke()

            // 3) 头/背
            ctx.beginPath()
            ctx.moveTo(18, 64)
            ctx.bezierCurveTo(6, 54, 6, 32, 18, 26)
            ctx.bezierCurveTo(22, 14, 30, 9, 38, 12)
            ctx.bezierCurveTo(45, 15, 48, 21, 49, 27)
            ctx.bezierCurveTo(50, 21, 53, 14, 60, 12)
            ctx.bezierCurveTo(68, 9, 76, 14, 80, 26)
            ctx.bezierCurveTo(92, 32, 92, 54, 80, 64)
            ctx.lineTo(75, 61)
            ctx.lineTo(70, 64)
            ctx.lineTo(64, 61)
            ctx.lineTo(58, 64)
            ctx.lineTo(52, 61)
            ctx.lineTo(46, 64)
            ctx.lineTo(40, 61)
            ctx.lineTo(34, 64)
            ctx.lineTo(28, 61)
            ctx.closePath()
            ctx.fillStyle = FUR
            ctx.fill()
            ctx.strokeStyle = OUT
            ctx.lineWidth = 4
            ctx.stroke()

            // 4) 蓝色标记
            ctx.strokeStyle = BLU
            ctx.lineWidth = 6
            ctx.beginPath()
            ctx.moveTo(45, 33)
            ctx.lineTo(48, 46)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(55, 31)
            ctx.lineTo(58, 44)
            ctx.stroke()

            // 5) 小波浪
            ctx.strokeStyle = OUT
            ctx.lineWidth = 3
            ctx.beginPath()
            ctx.moveTo(44, 47)
            ctx.bezierCurveTo(47, 54, 50, 54, 52, 48)
            ctx.bezierCurveTo(54, 54, 57, 54, 60, 47)
            ctx.stroke()
        }
    }
}
