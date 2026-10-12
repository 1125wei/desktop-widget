import QtQuick

// 一张卡片的「外壳」：决定尺寸 → 按类型装载视觉 → 编辑态的虚线框与拖动。
// 桌面组件（Main.qml，card = 这张卡自己的后端）与设置界面「实时预览」
// （card = Settings.previewCard）共用同一份，保证所见即所得。
Item {
    id: slot

    property var card: null          // CardBackend*
    property bool interactive: true  // 预览里设 false：不给拖、不画虚线框和提示胶囊

    readonly property bool isAnniv: card !== null && card.type === "anniversary"
    readonly property real cardW: card !== null ? card.cardWidth : 312
    // 各卡片的设计稿基准（窄尺寸用专门布局而不是把设计稿整体缩小，否则字号一起被缩没）
    readonly property real designW: isAnniv ? 320 : (cardW < 420 ? 320 : 470)
    readonly property real designH: isAnniv ? 150 : (cardW < 420 ? 150 : 206)

    width: Math.round(cardW)
    height: Math.round(cardW * designH / designW)

    readonly property bool editing: interactive && card !== null && card.editing
    readonly property real cornerRadius: loader.item ? loader.item.cornerRadius : 18

    Loader {
        id: loader
        anchors.fill: parent
        sourceComponent: slot.card === null ? null : (slot.isAnniv ? annivComp : countComp)
    }
    Component {
        id: countComp
        CountdownCard { card: slot.card }
    }
    Component {
        id: annivComp
        AnniversaryCard { card: slot.card }
    }

    // ==================== 编辑态：虚线框 + 拖动 + 提示胶囊 ====================
    Canvas {
        anchors.fill: parent
        anchors.margins: 3
        visible: slot.editing
        onVisibleChanged: if (visible)
            requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = "#2F88FF"
            ctx.lineWidth = 2
            ctx.setLineDash([7, 5])
            var r = slot.cornerRadius - 2
            var w = width
            var h = height
            ctx.beginPath()
            ctx.moveTo(r, 0)
            ctx.lineTo(w - r, 0)
            ctx.arcTo(w, 0, w, r, r)
            ctx.lineTo(w, h - r)
            ctx.arcTo(w, h, w - r, h, r)
            ctx.lineTo(r, h)
            ctx.arcTo(0, h, 0, h - r, r)
            ctx.lineTo(0, r)
            ctx.arcTo(0, 0, r, 0, r)
            ctx.closePath()
            ctx.stroke()
        }
    }

    // 贴在图标层之下时点不到卡片 → 必须在编辑模式里才能拖动
    MouseArea {
        anchors.fill: parent
        enabled: slot.editing
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        onPressed: function (mouse) {
            if (mouse.button === Qt.RightButton)
                slot.card.finishEditing()
            else
                slot.card.startDrag()
        }
        onPositionChanged: function (mouse) {
            if (pressed)
                slot.card.dragMove()
        }
    }

    Rectangle {
        visible: slot.editing
        anchors.horizontalCenter: parent.horizontalCenter
        y: 8
        width: pillRow.implicitWidth + 24
        height: 26
        radius: 13
        color: "#2F88FF"
        opacity: 0.96

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 12
            Text {
                text: "拖动摆放位置"
                color: "white"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 12
                renderType: Text.NativeRendering
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "完成"
                color: "white"
                font.family: "Microsoft YaHei UI"
                font.pixelSize: 12
                font.bold: true
                renderType: Text.NativeRendering
                anchors.verticalCenter: parent.verticalCenter
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: slot.card.finishEditing()
                }
            }
        }
    }
}
