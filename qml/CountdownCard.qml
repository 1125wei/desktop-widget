import QtQuick

// 下班倒计时卡片的**视觉本体**（尺寸由外层 CardSlot 给定，本组件不再自己算宽高）。
// 两种布局按宽度切换：
//   · 宽版（width >= 420）：参考截图 1:1 设计稿 470x206（含右侧形象位），整体等比缩放
//   · 窄版（width <  420）：320x150 专用布局 —— 不要空的形象位，把宽度让给文字，
//                            字号相对更大（"缩小后看不清"就是等比缩小把字号一起缩没了）
// 清晰度：文字统一 Text.NativeRendering（Windows 原生字形光栅化，实测比默认渲染锐 ~50%+）。
Item {
    id: root

    property var card: null

    readonly property bool narrow: width < 420
    readonly property real baseW: narrow ? 320 : 470
    readonly property real baseH: narrow ? 150 : 206
    readonly property real uiScale: width / baseW
    readonly property real cornerRadius: narrow ? 18 : 22

    readonly property color cardColor: card.cardColor
    readonly property color fontColor: card.fontColor
    readonly property color countdownColor: card.countdownColor
    readonly property color miniCardColor: card.miniCardColor
    readonly property color miniLabelColor: card.miniLabelColor
    readonly property color miniValueColor: card.miniValueColor
    readonly property string fontFamily: card.fontFamily

    // 四宫格（两种布局共用，尺寸/字号由参数传入）
    component MiniGrid: Row {
        id: grid
        property real cardW: 76
        property real cardH: 66
        property real cardRadius: 9
        property real fsLabel: 13
        property real fsValue: 19
        property real fsUnit: 12
        spacing: 6
        Repeater {
            model: root.card.miniCards
            delegate: Rectangle {
                width: grid.cardW
                height: grid.cardH
                radius: grid.cardRadius
                color: root.miniCardColor
                Column {
                    anchors.centerIn: parent
                    spacing: -2
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        font.family: root.fontFamily
                        font.pixelSize: grid.fsLabel
                        color: root.miniLabelColor
                        renderType: Text.NativeRendering
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.value
                        font.family: root.fontFamily
                        font.pixelSize: grid.fsValue
                        font.weight: Font.DemiBold
                        color: root.miniValueColor
                        renderType: Text.NativeRendering
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.unit
                        font.family: root.fontFamily
                        font.pixelSize: grid.fsUnit
                        color: root.miniLabelColor
                        renderType: Text.NativeRendering
                    }
                }
            }
        }
    }

    // ==================== 窄版布局（320x150 设计稿）====================
    Item {
        id: narrowContent
        width: 320
        height: 150
        scale: root.uiScale
        transformOrigin: Item.TopLeft
        visible: root.narrow

        Rectangle {
            anchors.fill: narrowCard
            anchors.topMargin: 3
            radius: narrowCard.radius + 1
            color: "#14000000"
        }
        Rectangle {
            id: narrowCard
            anchors.fill: parent
            radius: 18
            color: root.cardColor
            border.width: 1
            border.color: "#1A000000"
        }
        Text {
            x: 18
            y: 11
            text: root.card.title
            font.family: root.fontFamily
            font.pixelSize: 15
            color: root.fontColor
            renderType: Text.NativeRendering
        }
        Text {
            x: 16
            y: 31
            text: root.card.countdown
            font.family: root.fontFamily
            font.pixelSize: 40
            font.weight: Font.DemiBold
            font.letterSpacing: 1
            color: root.countdownColor
            renderType: Text.NativeRendering
        }
        MiniGrid {
            x: 19
            y: 97
            cardW: 66
            cardH: 46
            cardRadius: 8
            fsLabel: 11
            fsValue: 16
            fsUnit: 10
        }

        // 窄版也有卡通形象：放在倒计时数字右边、四个小卡上方那块空档
        // （尺寸是量过的：数字最宽到 x≈200，小卡从 y=97 起，所以这里 226..314 / 8..92 不会碰）
        Mascot {
            x: 226
            y: 8
            source: root.card.mascotMode
            width: 88
            height: 84
            visible: root.card.showMascot
        }
    }

    // ==================== 宽版布局（470x206 设计稿）====================
    Item {
        id: wideContent
        width: 470
        height: 206
        scale: root.uiScale
        transformOrigin: Item.TopLeft
        visible: !root.narrow

        Rectangle {
            anchors.fill: wideCard
            anchors.topMargin: 3
            radius: wideCard.radius + 1
            color: "#14000000"
        }
        Rectangle {
            id: wideCard
            anchors.fill: parent
            radius: 22
            color: root.cardColor
            border.width: 1
            border.color: "#1A000000"
        }
        Text {
            x: 26
            y: 18
            text: root.card.title
            font.family: root.fontFamily
            font.pixelSize: 19
            color: root.fontColor
            renderType: Text.NativeRendering
        }
        Text {
            x: 24
            y: 40
            text: root.card.countdown
            font.family: root.fontFamily
            font.pixelSize: 52
            font.weight: Font.DemiBold
            font.letterSpacing: 1
            color: root.countdownColor
            renderType: Text.NativeRendering
        }

        // 右侧形象位：卡通猫（矢量自绘，见 Mascot.qml；可在设置里关掉）
        Mascot {
            x: 356
            y: 44
            width: 112
            height: 128
            source: root.card.mascotMode
            visible: root.card.showMascot
        }

        MiniGrid {
            x: 24
            y: 126
            cardW: 76
            cardH: 66
            cardRadius: 9
            fsLabel: 13
            fsValue: 19
            fsUnit: 12
        }
    }
}
