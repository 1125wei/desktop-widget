import QtQuick

// 纪念日卡片的视觉本体（参考手机版"纪念日"组件）：
//   小字事件名 + 大号天数 + 底部起始日期，背景支持 纯色 / 内置月夜 / 自定义图片
// 设计稿 320x150，按实际宽度整体缩放（文字用 scale 变换仍保持锐利 —— 已在本项目
// 倒计时卡上实测过：Qt 是按设备空间重栅格化字形的）。
// 图片背景的圆角：纯色/渐变用 Rectangle{radius} 即可，但图片会被画出圆角外
// （Rectangle 不裁子项、Canvas 的 clip() 对图片也无效）→ 用 Canvas + destination-out 擦角。
Item {
    id: root

    property var card: null

    readonly property real baseW: 320
    readonly property real baseH: 150
    readonly property real uiScale: width / baseW
    readonly property real cornerRadius: 18

    readonly property bool imageMode: card.bgMode === "image" && card.hasImage
    // 没选图片但选了"图片"模式 → 退回内置月夜，不留空白
    readonly property bool moonMode: card.bgMode === "moon" || (card.bgMode === "image" && !card.hasImage)
    readonly property color fg: card.fontColor
    readonly property string fontFamily: card.fontFamily

    Item {
        id: content
        width: root.baseW
        height: root.baseH
        scale: root.uiScale
        transformOrigin: Item.TopLeft

        // 背景容器
        Item {
            id: bg
            anchors.fill: parent

            Rectangle {
                anchors.fill: parent
                radius: root.cornerRadius
                color: root.imageMode ? "transparent" : root.card.cardColor
                border.width: root.imageMode ? 0 : 1
                border.color: "#1A000000"
            }

            // 内置月夜背景：深蓝渐变 + 右侧一团柔光当月亮
            Rectangle {
                anchors.fill: parent
                radius: root.cornerRadius
                visible: root.moonMode
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#24455F" }
                    GradientStop { position: 0.55; color: "#152B41" }
                    GradientStop { position: 1.0; color: "#0A1420" }
                }
            }
            Item {
                anchors.fill: parent
                visible: root.moonMode
                Rectangle {
                    x: 196
                    y: 6
                    width: 112
                    height: 112
                    radius: 56
                    color: "#FFFFFF"
                    opacity: 0.07
                }
                Rectangle {
                    x: 212
                    y: 22
                    width: 80
                    height: 80
                    radius: 40
                    color: "#FFFFFF"
                    opacity: 0.12
                }
                Rectangle {
                    x: 224
                    y: 34
                    width: 56
                    height: 56
                    radius: 28
                    color: "#EAF1F8"
                    opacity: 0.88
                }
                // 陨石坑（让它像月亮而不是白圆点）
                Rectangle {
                    x: 234
                    y: 44
                    width: 11
                    height: 11
                    radius: 6
                    color: "#C8D6E4"
                    opacity: 0.75
                }
                Rectangle {
                    x: 252
                    y: 62
                    width: 15
                    height: 15
                    radius: 8
                    color: "#C8D6E4"
                    opacity: 0.6
                }
                Rectangle {
                    x: 240
                    y: 72
                    width: 8
                    height: 8
                    radius: 4
                    color: "#C8D6E4"
                    opacity: 0.6
                }
            }

            // 自定义背景图（Canvas 绘制：cover 铺满 + 压暗 + destination-out 擦圆角）
            // 踩坑记录：① Rectangle{radius} 不裁子项，图片会画出圆角外
            //          ② Canvas 的 ctx.clip() 对 fill 有效，但对 drawImage **无效**（Qt 绕过裁剪路径）
            //          ③ MultiEffect{maskEnabled;maskSource} 作 layer.effect 也可以实现同样的圆角
            //             （实测两者像素差 0.3%，只差抗锯齿边）——这里选 Canvas 是为了不依赖 QtQuick.Effects
            Image {
                id: srcImage
                source: root.imageMode ? root.card.bgImage : ""
                visible: false
                asynchronous: true
                cache: true
                onStatusChanged: if (status === Image.Ready)
                    bgCanvas.requestPaint()
            }
            Canvas {
                id: bgCanvas
                anchors.fill: parent
                visible: root.imageMode
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var r = root.cornerRadius
                    var w = width
                    var h = height
                    // 1) 先按 cover 方式铺满（等比放大后居中裁剪），避免拉伸变形
                    var iw = srcImage.sourceSize.width
                    var ih = srcImage.sourceSize.height
                    if (iw > 0 && ih > 0 && srcImage.status === Image.Ready) {
                        var s = Math.max(w / iw, h / ih)
                        var dw = iw * s
                        var dh = ih * s
                        ctx.drawImage(root.card.bgImage, (w - dw) / 2, (h - dh) / 2, dw, dh)
                    }
                    // 2) 压暗，保证白字可读
                    ctx.fillStyle = "rgba(0,0,0,0.28)"
                    ctx.fillRect(0, 0, w, h)
                    // 3) 圆角：用 destination-out 反向擦掉四角
                    //    为什么不用 ctx.clip()：Canvas 的 clip() 对 fill 有效、但对**图片**不生效
                    //    （Qt 的 Canvas 在 drawImage 时会绕过裁剪路径；item 版和 URL 版都一样）。
                    //    这里先整幅画图再擦角，结果与 MultiEffect 圆角遮罩一致（实测像素差 0.3%，只差抗锯齿边）。
                    ctx.globalCompositeOperation = "destination-out"
                    ctx.fillStyle = "rgba(0,0,0,1)"
                    // 左上
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(r, 0)
                    ctx.arc(r, r, r, -Math.PI / 2, -Math.PI, true)
                    ctx.lineTo(0, 0)
                    ctx.closePath()
                    ctx.fill()
                    // 右上
                    ctx.beginPath()
                    ctx.moveTo(w, 0)
                    ctx.lineTo(w - r, 0)
                    ctx.arc(w - r, r, r, -Math.PI / 2, 0, false)
                    ctx.lineTo(w, 0)
                    ctx.closePath()
                    ctx.fill()
                    // 右下
                    ctx.beginPath()
                    ctx.moveTo(w, h)
                    ctx.lineTo(w, h - r)
                    ctx.arc(w - r, h - r, r, 0, Math.PI / 2, false)
                    ctx.lineTo(w, h)
                    ctx.closePath()
                    ctx.fill()
                    // 左下
                    ctx.beginPath()
                    ctx.moveTo(0, h)
                    ctx.lineTo(0, h - r)
                    ctx.arc(r, h - r, r, Math.PI, Math.PI / 2, true)
                    ctx.lineTo(0, h)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        // 阴影（描边放在最后，避免被背景盖住）
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 3
            radius: root.cornerRadius + 1
            color: "#14000000"
            z: -1
        }
        Rectangle {
            anchors.fill: parent
            radius: root.cornerRadius
            color: "transparent"
            border.width: 1
            border.color: "#1A000000"
            visible: !root.imageMode
        }

        Text {
            x: 18
            y: 12
            text: root.card.eventName
            font.family: root.fontFamily
            font.pixelSize: 13
            color: root.fg
            opacity: 0.88
            renderType: Text.NativeRendering
        }

        Text {
            id: bigNum
            x: 16
            y: 29
            text: root.card.bigNumber
            font.family: root.fontFamily
            font.pixelSize: 46
            font.weight: Font.DemiBold
            color: root.fg
            renderType: Text.NativeRendering
        }
        Text {
            id: unitText
            x: bigNum.x + bigNum.implicitWidth + 5
            // 用 baselineOffset 对齐基线（比手工估 y 精确）
            y: bigNum.y + bigNum.baselineOffset - baselineOffset
            text: root.card.unitText
            font.family: root.fontFamily
            font.pixelSize: 16
            color: root.fg
            opacity: 0.9
            renderType: Text.NativeRendering
        }

        Text {
            x: 18
            y: 116
            text: root.card.dateText
            font.family: root.fontFamily
            font.pixelSize: 12
            color: root.fg
            opacity: 0.72
            renderType: Text.NativeRendering
        }

        // 月亮前面坐一只小猫（可在设置里关掉）
        Mascot {
            x: 248
            y: 62
            width: 70
            height: 82
            source: root.card.mascotMode
            visible: root.card.showMascot
        }
    }
}
