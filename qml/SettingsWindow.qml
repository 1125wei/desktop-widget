import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

// 设置界面：复刻手机版那个深色长面板（左侧实时预览 + 右侧一列配置）
// · 左侧预览 = 桌面组件的**同一份 CardSlot**，用 Settings.previewCard（当前选中的卡片）
// · 多张卡片：预览下方的圆点用来切换要配置的卡片，「＋」新增，「删除」移除
// · 所有改动即时生效（SettingsBackend → ConfigStore → WidgetManager.reloadAll）
Item {
    id: root
    width: 940
    height: 720

    readonly property color bg: "#131315"
    readonly property color rowBg: "#2C2C2E"
    readonly property color fieldBg: "#1C1C1E"
    readonly property color chipBg: "#303034"
    readonly property color chipBgActive: "#4E4E52"
    readonly property color fg: "#FFFFFF"
    readonly property color fgDim: "#9A9AA0"
    readonly property color accent: "#2F88FF"
    readonly property string uiFont: "Microsoft YaHei UI"

    readonly property bool isCountdown: Settings.cardType !== "anniversary"
    readonly property bool isAnniv: Settings.cardType === "anniversary"

    // 参考截图里的两套色板
    readonly property var bgPalette: ["#FFFFFF", "#FFC93C", "#FF3B30", "#8D6E63", "#8BC34A", "#1A4E8A",
                                      "#D7C49E", "#FFFDF0", "#7B1F2B", "#E53935", "#1E88E5", "#7ED6C6",
                                      "#4A7FA5", "#6D4C41", "#C9A227", "#F06292",
                                      "#16283C", "#24455F"]
    readonly property var fontPalette: ["#FFFFFF", "#1F1F1F", "#2B2B2B", "#5A5A5A", "#2F88FF", "#FF6B9A",
                                        "#7ED0F5", "#8FA8F0", "#F2A0C0", "#C3A6F0", "#F5E6A8", "#EAF7C0",
                                        "#B8E6C9", "#FF8A65"]

    property string pickerTarget: "card"
    property string pickerResult: "#FFFFFF"
    readonly property bool isLunar: Settings.isLunarMode
    readonly property string modeHint: Settings.annivMode === "countdown" ? "倒计时：还有多少天"
                                        : (Settings.annivMode === "lunar" ? "农历节日：按农历自动算下一个"
                                                                          : "纪念日：已经多少天")

    // 自检：DW_OPEN_DATE_PICKER=1 → 打开设置界面后自动弹出日历（抓图核对用）
    Component.onCompleted: if (Settings.autoOpenDatePicker)
        datePicker.openFor(Settings.date)

    Rectangle {
        anchors.fill: parent
        color: root.bg
    }

    // ==================== 自绘小组件 ====================
    component Chip: Rectangle {
        id: chip
        property string text: ""
        property bool checked: false
        property bool bold: false
        signal clicked()
        width: Math.max(42, chipLabel.implicitWidth + 20)
        height: 28
        radius: 8
        color: chip.checked ? root.chipBgActive : root.chipBg
        border.width: chip.checked ? 1.5 : 0
        border.color: "#FFFFFF"
        Text {
            id: chipLabel
            anchors.centerIn: parent
            text: chip.text
            color: root.fg
            font.pixelSize: 12
            font.family: root.uiFont
            font.bold: chip.bold
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.clicked()
        }
    }

    component Dot: Item {
        id: dot
        property color dotColor: "#FFFFFF"
        property bool selected: false
        property bool rainbow: false
        signal clicked()
        width: 26
        height: 26
        Gradient {
            id: rainbowGradient
            GradientStop { position: 0.0; color: "#FF3B30" }
            GradientStop { position: 0.2; color: "#FFC93C" }
            GradientStop { position: 0.4; color: "#8BC34A" }
            GradientStop { position: 0.6; color: "#2F88FF" }
            GradientStop { position: 0.8; color: "#8FA8F0" }
            GradientStop { position: 1.0; color: "#FF6B9A" }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 20
            height: 20
            radius: 10
            color: dot.rainbow ? "transparent" : dot.dotColor
            gradient: dot.rainbow ? rainbowGradient : null
            border.width: (!dot.rainbow && dot.dotColor === "#FFFFFF") ? 1 : 0
            border.color: "#6E6E74"
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: dot.selected ? 2 : 0
            border.color: "#FFFFFF"
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: dot.clicked()
        }
    }

    component Seg: Rectangle {
        id: seg
        property string text: ""
        property bool checked: false
        property bool usable: true
        signal clicked()
        width: segLabel.implicitWidth + 22
        height: 26
        radius: 8
        color: "transparent"
        border.width: seg.checked ? 1.5 : 0
        border.color: "#FFFFFF"
        opacity: seg.usable ? 1.0 : 0.35
        Text {
            id: segLabel
            anchors.centerIn: parent
            text: seg.text
            color: seg.checked ? root.fg : root.fgDim
            font.pixelSize: 12
            font.family: root.uiFont
        }
        MouseArea {
            anchors.fill: parent
            enabled: seg.usable
            cursorShape: Qt.PointingHandCursor
            onClicked: seg.clicked()
        }
    }

    // 一行式配置行：左边标签，右边控件（右对齐）
    component SettingRow: Rectangle {
        id: srow
        property string label: ""
        property int labelWidth: 96
        default property alias content: holder.data
        width: parent ? parent.width : 0
        implicitHeight: 64
        radius: 14
        color: root.rowBg
        Text {
            id: srowLabel
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            text: srow.label
            color: root.fg
            font.pixelSize: 16
            font.family: root.uiFont
        }
        Item {
            id: holder
            x: 18 + srow.labelWidth
            anchors.verticalCenter: parent.verticalCenter
            width: srow.width - x - 18
            height: srow.height
        }
    }

    // 两行式配置行：标签在左上，控件铺满下方（放工作日/色板这种宽控件）
    component BlockRow: Rectangle {
        id: brow
        property string label: ""
        default property alias content: bholder.data
        property alias trailing: trailingHolder.data
        width: parent ? parent.width : 0
        implicitHeight: 100
        radius: 14
        color: root.rowBg
        Text {
            id: browLabel
            x: 18
            y: 14
            text: brow.label
            color: root.fg
            font.pixelSize: 16
            font.family: root.uiFont
        }
        Item {
            id: trailingHolder
            anchors.right: parent.right
            anchors.rightMargin: 18
            y: 14
            height: 28
            width: childrenRect.width
        }
        Item {
            id: bholder
            x: 18
            y: 48
            width: brow.width - 36
            height: brow.height - 60
        }
    }

    // 主按钮（选择图片 / 删除此组件 之类）
    component Pill: Rectangle {
        id: pill
        property string text: ""
        property bool primary: false
        signal clicked()
        width: Math.max(76, pillLabel.implicitWidth + 30)
        height: 34
        radius: 17
        color: pill.primary ? root.accent : root.chipBg
        Text {
            id: pillLabel
            anchors.centerIn: parent
            text: pill.text
            color: "white"
            font.pixelSize: 13
            font.family: root.uiFont
            font.bold: pill.primary
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: pill.clicked()
        }
    }

    // 小方形按钮（日历导航用）
    component MiniBtn: Rectangle {
        id: mb
        property string text: ""
        signal clicked()
        width: 36
        height: 30
        radius: 8
        color: mbMouse.containsMouse ? root.chipBgActive : root.chipBg
        Text {
            anchors.centerIn: parent
            text: mb.text
            color: root.fg
            font.pixelSize: 13
            font.family: root.uiFont
        }
        MouseArea {
            id: mbMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: mb.clicked()
        }
    }

    // 深色下拉（设置面板里反复用到，抽出来免得把 popup 样板复制好几遍）
    component DarkCombo: ComboBox {
        id: dc
        property int boxWidth: 140
        width: boxWidth
        height: 36
        background: Rectangle {
            radius: 9
            color: root.fieldBg
        }
        contentItem: Text {
            // 窄框（<120）要把内边距收小，否则文本区被箭头+内边距挤成 0 宽，看起来就是"空框"
            leftPadding: dc.boxWidth < 120 ? 8 : 12
            rightPadding: dc.boxWidth < 120 ? 10 : 22
            text: dc.displayText
            color: root.fg
            font.pixelSize: 14
            font.family: root.uiFont
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        delegate: ItemDelegate {
            required property var modelData
            width: dc.width
            contentItem: Text {
                text: modelData
                color: root.fg
                font.pixelSize: 14
                leftPadding: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.highlighted ? root.chipBgActive : "transparent"
            }
        }
        popup: Popup {
            y: dc.height + 4
            width: dc.width
            implicitHeight: Math.min(dc.count, 9) * 34 + 12
            padding: 6
            background: Rectangle {
                color: "#232326"
                radius: 10
                border.width: 1
                border.color: "#3A3A3C"
            }
            contentItem: ListView {
                clip: true
                implicitHeight: Math.min(dc.count, 9) * 34
                model: dc.popup.visible ? dc.delegateModel : null
                currentIndex: dc.highlightedIndex
            }
        }
    }

    // ==================== 标题栏（自绘，可拖动）====================
    Rectangle {
        id: titleBar
        width: parent.width
        height: 46
        color: "#1B1B1E"

        Text {
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            text: "桌面组件 · 设置"
            color: root.fg
            font.pixelSize: 15
            font.family: root.uiFont
        }

        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 50
            property point lastGlobal
            onPressed: lastGlobal = mapToGlobal(mouse.x, mouse.y)
            onPositionChanged: {
                var g = mapToGlobal(mouse.x, mouse.y)
                Window.window.x += g.x - lastGlobal.x
                Window.window.y += g.y - lastGlobal.y
                lastGlobal = g
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            radius: 14
            color: closeArea.containsMouse ? "#E5484D" : root.rowBg
            Text {
                anchors.centerIn: parent
                text: "✕"
                color: root.fg
                font.pixelSize: 13
            }
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Settings.closeWindow()
            }
        }
    }

    // ==================== 主体 ====================
    Row {
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // ---------- 左：实时预览 + 卡片选择 ----------
        Item {
            width: 340
            height: parent.height
            Rectangle {
                anchors.fill: parent
                anchors.margins: 16
                radius: 18
                color: "#1B1B1E"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 38
                text: "实时预览"
                color: root.fgDim
                font.pixelSize: 15
                font.family: root.uiFont
            }
            Item {
                id: previewHolder
                anchors.horizontalCenter: parent.horizontalCenter
                y: 116
                // 预览：按真实宽度渲染卡片（保证窄/宽两种布局与实际一致），再整体缩放到面板内
                readonly property real fit: Math.min(1.0, 300 / Math.max(1, Settings.cardWidth))
                width: Math.round(previewCard.width * fit)
                height: Math.round(previewCard.height * fit)
                CardSlot {
                    id: previewCard
                    card: Settings.previewCard
                    interactive: false // 预览里不给拖、不画虚线框
                    scale: previewHolder.fit
                    transformOrigin: Item.TopLeft
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: -20
                    text: Settings.cardWidth + " px" + (previewHolder.fit < 1 ? "（预览已缩小）" : "")
                    color: root.fgDim
                    font.pixelSize: 11
                    font.family: root.uiFont
                }
            }

            // 卡片选择：圆点 + 新增 + 删除（对应手机版那个小圆点分页器）
            Row {
                id: cardSelector
                anchors.horizontalCenter: parent.horizontalCenter
                y: 320
                spacing: 8
                Repeater {
                    model: Settings.cardCount
                    delegate: Rectangle {
                        width: index === Settings.currentIndex ? 22 : 9
                        height: 9
                        radius: 4.5
                        color: index === Settings.currentIndex ? root.accent : "#5A5A60"
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Settings.currentIndex = index
                        }
                    }
                }
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 344
                spacing: 10

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Settings.cardNames.length > 0 ? Settings.cardNames[Settings.currentIndex] : ""
                    color: root.fg
                    font.pixelSize: 13
                    font.family: root.uiFont
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8
                    Pill { text: "＋ 倒计时"; onClicked: Settings.addCard("countdown") }
                    Pill { text: "＋ 纪念日"; onClicked: Settings.addCard("anniversary") }
                    Pill {
                        text: "删除"
                        onClicked: removeConfirm.open()
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 26
                width: parent.width - 60
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: "改动即时生效，桌面上的组件同步变化\n共 " + Settings.cardCount + " 个组件"
                color: root.fgDim
                font.pixelSize: 12
                lineHeight: 1.4
                font.family: root.uiFont
            }
        }

        // ---------- 右：配置列 ----------
        Item {
            id: rightPane
            width: parent.width - 340
            height: parent.height

            Flickable {
                id: flick
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: footer.top
                contentWidth: width
                contentHeight: column.implicitHeight + 36
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    id: column
                    x: 0
                    y: 18
                    width: flick.width - 40
                    spacing: 12

                    SettingRow {
                        label: "组件名称"
                        TextField {
                            id: nameField
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 300
                            horizontalAlignment: TextInput.AlignRight
                            color: root.fg
                            font.pixelSize: 16
                            font.family: root.uiFont
                            background: null
                            selectByMouse: true
                            Component.onCompleted: text = Settings.cardName
                            Connections {
                                target: Settings
                                function onStyleChanged() {
                                    if (!nameField.activeFocus)
                                        nameField.text = Settings.cardName
                                }
                                function onCurrentChanged() {
                                    nameField.text = Settings.cardName
                                }
                            }
                            onEditingFinished: Settings.cardName = text
                        }
                    }

                    SettingRow {
                        label: "组件大小"
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12
                            Slider {
                                id: widthSlider
                                width: 250
                                from: 240
                                to: 760
                                stepSize: 4
                                Component.onCompleted: value = Settings.cardWidth
                                Connections {
                                    target: Settings
                                    function onCardWidthChanged() {
                                        if (!widthSlider.pressed)
                                            widthSlider.value = Settings.cardWidth
                                    }
                                    function onCurrentChanged() {
                                        widthSlider.value = Settings.cardWidth
                                    }
                                }
                                onMoved: Settings.cardWidth = value
                            }
                            Text {
                                text: Settings.cardWidth + " px"
                                color: root.fg
                                font.pixelSize: 14
                                width: 66
                                horizontalAlignment: Text.AlignRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    // ==================== 倒计时卡专属 ====================
                    SettingRow {
                        label: "卡片文案"
                        visible: root.isCountdown
                        implicitHeight: visible ? 64 : 0
                        TextField {
                            id: titleField
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 300
                            horizontalAlignment: TextInput.AlignRight
                            color: root.fg
                            font.pixelSize: 16
                            font.family: root.uiFont
                            background: null
                            selectByMouse: true
                            Component.onCompleted: text = Settings.title
                            Connections {
                                target: Settings
                                function onTitleChanged() {
                                    if (!titleField.activeFocus)
                                        titleField.text = Settings.title
                                }
                                function onCurrentChanged() {
                                    titleField.text = Settings.title
                                }
                            }
                            onEditingFinished: Settings.title = text
                        }
                    }

                    BlockRow {
                        label: "工作日"
                        visible: root.isCountdown
                        implicitHeight: visible ? 100 : 0
                        Row {
                            spacing: 6
                            Repeater {
                                model: ["一", "二", "三", "四", "五", "六", "日"]
                                delegate: Chip {
                                    text: "周" + modelData
                                    checked: Settings.workDays[index] === true
                                    onClicked: Settings.toggleWorkDay(index)
                                }
                            }
                        }
                        trailing: Chip {
                            text: "工作日"
                            bold: true
                            checked: Settings.workdayPreset
                            onClicked: Settings.applyWorkdayPreset()
                        }
                    }

                    SettingRow {
                        label: "工作时间"
                        visible: root.isCountdown
                        implicitHeight: visible ? 64 : 0
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10
                            Rectangle {
                                width: 96
                                height: 36
                                radius: 9
                                color: root.fieldBg
                                TextField {
                                    id: startField
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 8
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: root.fg
                                    font.pixelSize: 15
                                    background: null
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    Component.onCompleted: text = Settings.workStart
                                    Connections {
                                        target: Settings
                                        function onTimeChanged() {
                                            if (!startField.activeFocus)
                                                startField.text = Settings.workStart
                                        }
                                        function onCurrentChanged() {
                                            startField.text = Settings.workStart
                                        }
                                    }
                                    onEditingFinished: {
                                        Settings.applyTimeInput("start", text)
                                        text = Settings.workStart
                                    }
                                }
                            }
                            Text {
                                text: "至"
                                color: root.fgDim
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Rectangle {
                                width: 96
                                height: 36
                                radius: 9
                                color: root.fieldBg
                                TextField {
                                    id: endField
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 8
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: root.fg
                                    font.pixelSize: 15
                                    background: null
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    Component.onCompleted: text = Settings.workEnd
                                    Connections {
                                        target: Settings
                                        function onTimeChanged() {
                                            if (!endField.activeFocus)
                                                endField.text = Settings.workEnd
                                        }
                                        function onCurrentChanged() {
                                            endField.text = Settings.workEnd
                                        }
                                    }
                                    onEditingFinished: {
                                        Settings.applyTimeInput("end", text)
                                        text = Settings.workEnd
                                    }
                                }
                            }
                        }
                    }

                    // ==================== 纪念日卡专属 ====================
                    BlockRow {
                        label: "模式"
                        visible: root.isAnniv
                        implicitHeight: visible ? 112 : 0
                        Row {
                            spacing: 8
                            Seg {
                                text: "倒计时"
                                checked: Settings.annivMode === "countdown"
                                onClicked: Settings.annivMode = "countdown"
                            }
                            Seg {
                                text: "纪念日"
                                checked: Settings.annivMode === "anniversary"
                                onClicked: Settings.annivMode = "anniversary"
                            }
                            Seg {
                                text: "农历节日"
                                checked: Settings.annivMode === "lunar"
                                onClicked: Settings.annivMode = "lunar"
                            }
                        }
                        // 实时告诉用户"按当前设置，卡片上会显示成什么"（与卡片同一套口径算出来）
                        Text {
                            anchors.top: parent.top
                            anchors.topMargin: 34
                            width: parent.width
                            elide: Text.ElideRight
                            text: root.modeHint + " · 卡片会显示：" + Settings.effectPreview
                            color: root.fgDim
                            font.pixelSize: 12
                            font.family: root.uiFont
                        }
                    }

                    SettingRow {
                        label: "事件名称"
                        visible: root.isAnniv
                        implicitHeight: visible ? 64 : 0
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10
                            TextField {
                                id: eventField
                                width: 200
                                horizontalAlignment: TextInput.AlignRight
                                color: root.fg
                                font.pixelSize: 16
                                font.family: root.uiFont
                                background: null
                                selectByMouse: true
                                Component.onCompleted: text = Settings.eventName
                                Connections {
                                    target: Settings
                                    function onAnniversaryChanged() {
                                        if (!eventField.activeFocus)
                                            eventField.text = Settings.eventName
                                    }
                                    function onCurrentChanged() {
                                        eventField.text = Settings.eventName
                                    }
                                }
                                onEditingFinished: Settings.eventName = text
                            }
                            Text {
                                text: "常用事件"
                                color: root.accent
                                font.pixelSize: 13
                                font.family: root.uiFont
                                anchors.verticalCenter: parent.verticalCenter
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: suggestPopup.open()
                                }
                            }
                        }
                    }

                    // 农历节日模式：选节日（或自定义农历月/日）
                    SettingRow {
                        label: "农历节日"
                        visible: root.isAnniv && root.isLunar
                        implicitHeight: visible ? 64 : 0
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            DarkCombo {
                                boxWidth: root.isLunar && Settings.lunarCustom ? 170 : 240
                                model: Settings.lunarEventLabels
                                currentIndex: Settings.lunarEventIndex
                                onActivated: Settings.lunarEventIndex = currentIndex
                            }
                            DarkCombo {
                                visible: Settings.lunarCustom
                                boxWidth: 92
                                model: Settings.lunarMonthLabels
                                currentIndex: Settings.lunarMonthIndex
                                onActivated: Settings.lunarMonthIndex = currentIndex
                            }
                            DarkCombo {
                                visible: Settings.lunarCustom
                                boxWidth: 92
                                model: Settings.lunarDayLabels
                                currentIndex: Settings.lunarDayIndex
                                onActivated: Settings.lunarDayIndex = currentIndex
                            }
                        }
                    }

                    SettingRow {
                        label: "日期"
                        visible: root.isAnniv && !root.isLunar
                        implicitHeight: visible ? 64 : 0
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Rectangle {
                                width: 140
                                height: 36
                                radius: 9
                                color: root.fieldBg
                                // ⚠️ 这里**故意不用 inputMask**：掩码会把"2027-3-5"原样放行、
                                // 又认为"2027-02-31"完全合法，出错时用户完全看不出问题
                                TextField {
                                    id: dateField
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 8
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: root.fg
                                    font.pixelSize: 15
                                    font.family: root.uiFont
                                    background: null
                                    selectByMouse: true
                                    placeholderText: "2027-3-5"
                                    Component.onCompleted: text = Settings.date
                                    Connections {
                                        target: Settings
                                        function onAnniversaryChanged() {
                                            if (!dateField.activeFocus)
                                                dateField.text = Settings.date
                                        }
                                        function onCurrentChanged() {
                                            dateField.text = Settings.date
                                        }
                                    }
                                    // 提交：宽松解析（2027-3-5 / 2027/03/05 / 2027年3月5日 都行）
                                    // 失败 → 明确回退成已存值 + 顶部提示，绝不留着用户打的字假装成功
                                    onEditingFinished: {
                                        var ok = Settings.applyDateInput(text)
                                        text = Settings.date
                                        if (!ok)
                                            dateHint.restart()
                                    }
                                }
                            }
                            // 日历选择（不想打字的用户直接点）
                            Rectangle {
                                width: 36
                                height: 36
                                radius: 9
                                color: calMouse.containsMouse ? root.chipBgActive : root.fieldBg
                                Text {
                                    anchors.centerIn: parent
                                    text: "📅"
                                    font.pixelSize: 15
                                }
                                MouseArea {
                                    id: calMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: datePicker.openFor(Settings.date)
                                }
                            }
                            // 重复规则只在倒计时模式有意义（纪念日模式就是"从那天起一直累加"）
                            ComboBox {
                                id: repeatCombo
                                width: 118
                                height: 36
                                visible: Settings.annivMode === "countdown"
                                model: Settings.repeatLabels
                                currentIndex: Settings.repeatIndex
                                onActivated: Settings.repeatIndex = currentIndex
                                background: Rectangle {
                                    radius: 9
                                    color: root.fieldBg
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: repeatCombo.displayText
                                    color: root.fg
                                    font.pixelSize: 14
                                    font.family: root.uiFont
                                    verticalAlignment: Text.AlignVCenter
                                }
                                delegate: ItemDelegate {
                                    required property var modelData
                                    required property int index
                                    width: repeatCombo.width
                                    contentItem: Text {
                                        text: modelData
                                        color: root.fg
                                        font.pixelSize: 14
                                        leftPadding: 12
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: parent.highlighted ? root.chipBgActive : "transparent"
                                    }
                                }
                                popup: Popup {
                                    y: repeatCombo.height + 4
                                    width: repeatCombo.width
                                    implicitHeight: repeatCombo.count * 34 + 12
                                    padding: 6
                                    background: Rectangle {
                                        color: "#232326"
                                        radius: 10
                                        border.width: 1
                                        border.color: "#3A3A3C"
                                    }
                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: repeatCombo.count * 34
                                        model: repeatCombo.popup.visible ? repeatCombo.delegateModel : null
                                        currentIndex: repeatCombo.highlightedIndex
                                    }
                                }
                            }
                        }
                    }

                    // 模式/日期组合不合适时的提醒（红字，随设置实时变）
                    Text {
                        width: parent.width
                        visible: root.isAnniv && Settings.dateWarning.length > 0
                        text: Settings.dateWarning
                        color: "#FF6B6B"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        lineHeight: 1.3
                        font.family: root.uiFont
                    }

                    // ==================== 背景（颜色 / 图片 / 月夜）====================
                    BlockRow {
                        label: "背景"
                        implicitHeight: 128
                        Grid {
                            visible: Settings.bgMode === "color"
                            columns: 11
                            spacing: 4
                            Repeater {
                                model: root.bgPalette
                                delegate: Dot {
                                    dotColor: modelData
                                    selected: Settings.cardColor.toUpperCase() === modelData
                                    onClicked: Settings.cardColor = modelData
                                }
                            }
                            Dot {
                                rainbow: true
                                selected: root.bgPalette.indexOf(Settings.cardColor.toUpperCase()) < 0
                                onClicked: root.openPicker("card", Settings.cardColor)
                            }
                        }
                        Row {
                            visible: Settings.bgMode === "image"
                            spacing: 12
                            Pill {
                                text: "选择图片…"
                                primary: true
                                onClicked: Settings.chooseBgImage()
                            }
                            Pill {
                                text: "清除"
                                onClicked: Settings.clearBgImage()
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 220
                                elide: Text.ElideMiddle
                                text: Settings.bgImageName.length > 0 ? Settings.bgImageName : "未选择图片"
                                color: root.fgDim
                                font.pixelSize: 12
                                font.family: root.uiFont
                            }
                        }
                        trailing: Row {
                            spacing: 6
                            Seg {
                                text: "颜色"
                                checked: Settings.bgMode === "color"
                                onClicked: Settings.bgMode = "color"
                            }
                            Seg {
                                text: "图片"
                                usable: root.isAnniv
                                checked: Settings.bgMode === "image"
                                onClicked: Settings.bgMode = "image"
                            }
                            Seg {
                                text: "月夜"
                                usable: root.isAnniv
                                checked: Settings.bgMode === "moon"
                                onClicked: Settings.bgMode = "moon"
                            }
                        }
                        // 月夜模式：用内置渐变，不需要选色/选图（给一行说明，避免这块看起来是空的）
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: Settings.bgMode === "moon"
                            text: "使用内置月夜渐变背景（深蓝夜空 + 月亮柔光）"
                            color: root.fgDim
                            font.pixelSize: 12
                            font.family: root.uiFont
                        }
                    }

                    BlockRow {
                        label: "信息格底色"
                        visible: root.isCountdown
                        implicitHeight: visible ? 74 : 0
                        Grid {
                            columns: 11
                            spacing: 4
                            Repeater {
                                model: ["#F2F2F2", "#EDEDED", "#E3E7EF", "#F6EFE3", "#E8F3EA", "#F0E8F5", "#FFF7D6", "#FFFFFF"]
                                delegate: Dot {
                                    dotColor: modelData
                                    selected: Settings.miniCardColor.toUpperCase() === modelData
                                    onClicked: Settings.miniCardColor = modelData
                                }
                            }
                            Dot {
                                rainbow: true
                                selected: ["#F2F2F2", "#EDEDED", "#E3E7EF", "#F6EFE3", "#E8F3EA", "#F0E8F5", "#FFF7D6", "#FFFFFF"].indexOf(Settings.miniCardColor.toUpperCase()) < 0
                                onClicked: root.openPicker("mini", Settings.miniCardColor)
                            }
                        }
                    }

                    BlockRow {
                        label: "字体颜色"
                        implicitHeight: 128
                        Grid {
                            columns: 11
                            spacing: 4
                            Repeater {
                                model: root.fontPalette
                                delegate: Dot {
                                    dotColor: modelData
                                    selected: Settings.fontColor.toUpperCase() === modelData
                                    onClicked: Settings.fontColor = modelData
                                }
                            }
                            Dot {
                                rainbow: true
                                selected: root.fontPalette.indexOf(Settings.fontColor.toUpperCase()) < 0
                                onClicked: root.openPicker("font", Settings.fontColor)
                            }
                        }
                    }

                    SettingRow {
                        label: "卡通形象"
                        implicitHeight: 64
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Seg {
                                text: "图片（你的猫）"
                                checked: Settings.showMascot && Settings.mascotMode === "image"
                                onClicked: {
                                    Settings.mascotMode = "image"
                                    Settings.showMascot = true
                                }
                            }
                            Seg {
                                text: "矢量自绘"
                                checked: Settings.showMascot && Settings.mascotMode === "vector"
                                onClicked: {
                                    Settings.mascotMode = "vector"
                                    Settings.showMascot = true
                                }
                            }
                            Seg {
                                text: "隐藏"
                                checked: !Settings.showMascot
                                onClicked: Settings.showMascot = false
                            }
                        }
                    }

                    SettingRow {
                        label: "字体"
                        ComboBox {
                            id: fontCombo
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 300
                            height: 36
                            model: Settings.fontFamilies
                            currentIndex: Math.max(0, Settings.fontFamilies.indexOf(Settings.fontFamily))
                            onActivated: Settings.fontFamily = currentText
                            background: Rectangle {
                                radius: 9
                                color: root.fieldBg
                            }
                            contentItem: Text {
                                leftPadding: 12
                                text: fontCombo.displayText
                                color: root.fg
                                font.pixelSize: 14
                                font.family: root.uiFont
                                verticalAlignment: Text.AlignVCenter
                            }
                            delegate: ItemDelegate {
                                required property var modelData
                                required property int index
                                width: fontCombo.width
                                contentItem: Text {
                                    text: modelData
                                    color: root.fg
                                    font.pixelSize: 14
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 12
                                }
                                background: Rectangle {
                                    color: parent.highlighted ? root.chipBgActive : "transparent"
                                }
                            }
                            popup: Popup {
                                y: fontCombo.height + 4
                                width: fontCombo.width
                                implicitHeight: 320
                                padding: 6
                                background: Rectangle {
                                    color: "#232326"
                                    radius: 10
                                    border.width: 1
                                    border.color: "#3A3A3C"
                                }
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: 300
                                    model: fontCombo.popup.visible ? fontCombo.delegateModel : null
                                    currentIndex: fontCombo.highlightedIndex
                                    ScrollBar.vertical: ScrollBar {}
                                }
                            }
                        }
                    }

                    BlockRow {
                        label: "显示更多"
                        visible: root.isCountdown
                        implicitHeight: visible ? 74 : 0
                        Row {
                            spacing: 8
                            Chip {
                                text: "发薪日"
                                checked: Settings.showPayday
                                onClicked: Settings.showPayday = !Settings.showPayday
                            }
                            Chip {
                                text: "距离周五"
                                checked: Settings.showFriday
                                onClicked: Settings.showFriday = !Settings.showFriday
                            }
                            Chip {
                                text: "下一个节日"
                                checked: Settings.showHoliday
                                onClicked: Settings.showHoliday = !Settings.showHoliday
                            }
                            Chip {
                                text: "今天收入"
                                checked: Settings.showEarnings
                                onClicked: Settings.showEarnings = !Settings.showEarnings
                            }
                        }
                    }

                    SettingRow {
                        label: "发薪日"
                        visible: root.isCountdown
                        implicitHeight: visible ? 64 : 0
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Text {
                                text: "每月"
                                color: root.fg
                                font.pixelSize: 15
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ComboBox {
                                id: paydayCombo
                                width: 96
                                height: 36
                                model: 31
                                currentIndex: Settings.payday - 1
                                displayText: (currentIndex + 1) + ""
                                onActivated: Settings.payday = currentIndex + 1
                                background: Rectangle {
                                    radius: 9
                                    color: root.fieldBg
                                }
                                contentItem: Text {
                                    leftPadding: 12
                                    text: paydayCombo.displayText
                                    color: root.fg
                                    font.pixelSize: 15
                                    verticalAlignment: Text.AlignVCenter
                                }
                                delegate: ItemDelegate {
                                    required property int index
                                    width: paydayCombo.width
                                    contentItem: Text {
                                        text: (index + 1) + ""
                                        color: root.fg
                                        font.pixelSize: 14
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 12
                                    }
                                    background: Rectangle {
                                        color: parent.highlighted ? root.chipBgActive : "transparent"
                                    }
                                }
                            }
                            Text {
                                text: "日"
                                color: root.fg
                                font.pixelSize: 15
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    SettingRow {
                        label: "每天的收入"
                        visible: root.isCountdown
                        implicitHeight: visible ? 64 : 0
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8
                            Rectangle {
                                width: 34
                                height: 34
                                radius: 17
                                color: root.fieldBg
                                Text {
                                    anchors.centerIn: parent
                                    text: "−"
                                    color: root.fg
                                    font.pixelSize: 18
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Settings.dailyIncome = Math.max(0, Settings.dailyIncome - 10)
                                }
                            }
                            Rectangle {
                                width: 120
                                height: 34
                                radius: 9
                                color: root.fieldBg
                                TextField {
                                    id: incomeField
                                    anchors.fill: parent
                                    horizontalAlignment: TextInput.AlignHCenter
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: root.fg
                                    font.pixelSize: 15
                                    background: null
                                    selectByMouse: true
                                    Component.onCompleted: text = Settings.dailyIncome.toFixed(0)
                                    Connections {
                                        target: Settings
                                        function onDailyIncomeChanged() {
                                            if (!incomeField.activeFocus)
                                                incomeField.text = Settings.dailyIncome.toFixed(0)
                                        }
                                        function onCurrentChanged() {
                                            incomeField.text = Settings.dailyIncome.toFixed(0)
                                        }
                                    }
                                    onEditingFinished: Settings.dailyIncome = Math.max(0, parseFloat(text) || 0)
                                }
                            }
                            Rectangle {
                                width: 34
                                height: 34
                                radius: 17
                                color: root.fieldBg
                                Text {
                                    anchors.centerIn: parent
                                    text: "＋"
                                    color: root.fg
                                    font.pixelSize: 16
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Settings.dailyIncome = Settings.dailyIncome + 10
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "配置文件：" + Settings.configPath
                        color: root.fgDim
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        font.family: root.uiFont
                    }
                }
            }

            // 固定底部按钮（不随配置列滚动）
            Row {
                id: footer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 92
                spacing: 12

                Rectangle {
                    width: 130
                    height: 52
                    radius: 26
                    color: root.rowBg
                    Text {
                        anchors.centerIn: parent
                        text: "恢复默认"
                        color: root.fg
                        font.pixelSize: 15
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Settings.resetDefaults()
                    }
                }
                Rectangle {
                    width: 150
                    height: 52
                    radius: 26
                    color: root.rowBg
                    Text {
                        anchors.centerIn: parent
                        text: "调整位置"
                        color: root.fg
                        font.pixelSize: 15
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Settings.editPosition()
                    }
                }
                Rectangle {
                    width: parent.width - 316
                    height: 52
                    radius: 26
                    color: root.accent
                    Text {
                        anchors.centerIn: parent
                        text: "完成"
                        color: "white"
                        font.pixelSize: 17
                        font.bold: true
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Settings.closeWindow()
                    }
                }
            }
        }
    }

    // ==================== 输入被拒时的顶部提示（不让用户以为"改了没反应"）====================
    Timer {
        id: dateHint
        interval: 2800
    }
    Rectangle {
        id: toast
        visible: dateHint.running
        anchors.horizontalCenter: parent.horizontalCenter
        y: 58
        width: toastLabel.implicitWidth + 34
        height: 36
        radius: 18
        color: "#E5484D"
        opacity: 0.97
        z: 50
        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: "日期无法识别，已恢复原值（可用 2027-3-5 / 2027/03/05 / 2027年3月5日）"
            color: "white"
            font.pixelSize: 13
            font.family: root.uiFont
        }
    }

    // ==================== 日期选择器（自绘深色日历，不用打字）====================
    Popup {
        id: datePicker
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 348
        height: 386
        padding: 14
        property int viewYear: 2026
        property int viewMonth: 0 // 0-11
        property int selYear: 2026
        property int selMonth: 0
        property int selDay: 1
        property var cells: []

        function pad2(n) {
            return (n < 10 ? "0" : "") + n
        }
        function rebuild() {
            var first = new Date(viewYear, viewMonth, 1)
            var startDow = (first.getDay() + 6) % 7 // 周一 = 0
            var dim = new Date(viewYear, viewMonth + 1, 0).getDate()
            var arr = []
            for (var i = 0; i < 42; i++) {
                var dn = i - startDow + 1
                arr.push({
                              "day": dn,
                              "cur": (dn >= 1 && dn <= dim),
                              "date": new Date(viewYear, viewMonth, dn)
                          })
            }
            cells = arr
        }
        function openFor(dateStr) {
            var parts = ("" + dateStr).split("-")
            var y = parseInt(parts[0])
            var m = parseInt(parts[1]) - 1
            var d = parseInt(parts[2])
            var now = new Date()
            if (isNaN(y) || isNaN(m) || isNaN(d)) {
                y = now.getFullYear()
                m = now.getMonth()
                d = now.getDate()
            }
            selYear = y
            selMonth = m
            selDay = d
            viewYear = y
            viewMonth = m
            rebuild()
            open()
        }
        function pick(dt) {
            Settings.date = dt.getFullYear() + "-" + pad2(dt.getMonth() + 1) + "-" + pad2(dt.getDate())
            close()
        }
        onViewYearChanged: rebuild()
        onViewMonthChanged: rebuild()

        background: Rectangle {
            color: "#1F1F22"
            radius: 16
            border.width: 1
            border.color: "#3A3A3C"
        }
        contentItem: Column {
            spacing: 8
            Row {
                spacing: 6
                MiniBtn {
                    text: "‹‹"
                    onClicked: datePicker.viewYear -= 1
                }
                MiniBtn {
                    text: "‹"
                    onClicked: {
                        if (datePicker.viewMonth === 0) {
                            datePicker.viewMonth = 11
                            datePicker.viewYear -= 1
                        } else {
                            datePicker.viewMonth -= 1
                        }
                    }
                }
                Text {
                    width: 132
                    horizontalAlignment: Text.AlignHCenter
                    anchors.verticalCenter: parent.verticalCenter
                    text: datePicker.viewYear + " 年 " + (datePicker.viewMonth + 1) + " 月"
                    color: root.fg
                    font.pixelSize: 15
                    font.family: root.uiFont
                }
                MiniBtn {
                    text: "›"
                    onClicked: {
                        if (datePicker.viewMonth === 11) {
                            datePicker.viewMonth = 0
                            datePicker.viewYear += 1
                        } else {
                            datePicker.viewMonth += 1
                        }
                    }
                }
                MiniBtn {
                    text: "››"
                    onClicked: datePicker.viewYear += 1
                }
            }
            Row {
                spacing: 2
                Repeater {
                    model: ["一", "二", "三", "四", "五", "六", "日"]
                    delegate: Text {
                        width: 44
                        height: 18
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        color: root.fgDim
                        font.pixelSize: 12
                        font.family: root.uiFont
                    }
                }
            }
            Grid {
                columns: 7
                rowSpacing: 2
                columnSpacing: 2
                Repeater {
                    model: datePicker.cells
                    delegate: Rectangle {
                        required property var modelData
                        readonly property var d: modelData.date
                        readonly property bool isSel: modelData.cur && d.getFullYear() === datePicker.selYear
                                                      && d.getMonth() === datePicker.selMonth
                                                      && modelData.day === datePicker.selDay
                        readonly property bool isToday: {
                            var t = new Date()
                            return modelData.cur && t.getFullYear() === d.getFullYear()
                                    && t.getMonth() === d.getMonth() && t.getDate() === d.getDate()
                        }
                        width: 44
                        height: 32
                        radius: 8
                        color: isSel ? root.accent : (todayMouse.containsMouse && modelData.cur ? root.chipBg : "transparent")
                        border.width: (isToday && !isSel) ? 1 : 0
                        border.color: root.accent
                        Text {
                            anchors.centerIn: parent
                            text: modelData.cur ? modelData.day : ""
                            color: isSel ? "white" : (modelData.cur ? root.fg : "#5A5A60")
                            font.pixelSize: 14
                            font.family: root.uiFont
                        }
                        MouseArea {
                            id: todayMouse
                            anchors.fill: parent
                            enabled: modelData.cur
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: datePicker.pick(d)
                        }
                    }
                }
            }
            Row {
                spacing: 10
                anchors.right: parent.right
                Pill {
                    text: "今天"
                    onClicked: {
                        var t = new Date()
                        datePicker.selYear = t.getFullYear()
                        datePicker.selMonth = t.getMonth()
                        datePicker.selDay = t.getDate()
                        datePicker.viewYear = t.getFullYear()
                        datePicker.viewMonth = t.getMonth()
                        datePicker.pick(t)
                    }
                }
                Pill {
                    text: "取消"
                    onClicked: datePicker.close()
                }
            }
        }
    }

    // ==================== 「常用事件」快捷选择 ====================
    Popup {
        id: suggestPopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 320
        implicitHeight: Math.min(420, suggestList.contentHeight + 70)
        padding: 14
        background: Rectangle {
            color: "#1F1F22"
            radius: 16
            border.width: 1
            border.color: "#3A3A3C"
        }
        contentItem: Column {
            spacing: 8
            Text {
                text: "常用事件"
                color: root.fg
                font.pixelSize: 15
                font.family: root.uiFont
            }
            ListView {
                id: suggestList
                width: 292
                height: Math.min(340, contentHeight)
                clip: true
                model: Settings.eventSuggestions
                delegate: Rectangle {
                    required property var modelData
                    width: suggestList.width
                    height: 36
                    radius: 9
                    color: sugMouse.containsMouse ? root.chipBg : "transparent"
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        color: root.fg
                        font.pixelSize: 14
                        font.family: root.uiFont
                    }
                    MouseArea {
                        id: sugMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Settings.applyEventSuggestion(modelData)
                            suggestPopup.close()
                        }
                    }
                }
            }
        }
    }

    // ==================== 删除确认 ====================
    Popup {
        id: removeConfirm
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 360
        height: 180
        padding: 18
        background: Rectangle {
            color: "#1F1F22"
            radius: 16
            border.width: 1
            border.color: "#3A3A3C"
        }
        contentItem: Column {
            spacing: 14
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "删除组件「" + Settings.cardName + "」？\n删除后桌面上的这张卡片会立刻消失（不可撤销）。"
                color: root.fg
                font.pixelSize: 14
                lineHeight: 1.4
                font.family: root.uiFont
            }
            Row {
                anchors.right: parent.right
                spacing: 12
                Rectangle {
                    width: 90
                    height: 36
                    radius: 18
                    color: root.rowBg
                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: root.fg
                        font.pixelSize: 14
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: removeConfirm.close()
                    }
                }
                Rectangle {
                    width: 90
                    height: 36
                    radius: 18
                    color: "#E5484D"
                    Text {
                        anchors.centerIn: parent
                        text: "删除"
                        color: "white"
                        font.pixelSize: 14
                        font.bold: true
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Settings.removeCurrentCard()
                            removeConfirm.close()
                        }
                    }
                }
            }
        }
    }

    // ==================== 自定义颜色（HSV 三滑块）====================
    function openPicker(target, currentColor) {
        pickerTarget = target
        var c = currentColor
        var h = c.hsvHue
        colorPicker.hue = (h < 0) ? 0 : h
        colorPicker.sat = c.hsvSaturation
        colorPicker.val = c.hsvValue
        colorPicker.open()
    }

    Popup {
        id: colorPicker
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 380
        height: 320
        padding: 18
        property real hue: 0
        property real sat: 1
        property real val: 1
        background: Rectangle {
            color: "#1F1F22"
            radius: 16
            border.width: 1
            border.color: "#3A3A3C"
        }
        contentItem: Column {
            spacing: 12
            Text {
                text: "自定义颜色"
                color: root.fg
                font.pixelSize: 15
                font.family: root.uiFont
            }
            Rectangle {
                width: 344
                height: 48
                radius: 10
                color: Qt.hsva(colorPicker.hue, colorPicker.sat, colorPicker.val, 1)
                border.width: 1
                border.color: "#3A3A3C"
            }
            Row {
                spacing: 10
                Text {
                    text: "色相"
                    color: root.fgDim
                    font.pixelSize: 13
                    width: 40
                    anchors.verticalCenter: parent.verticalCenter
                }
                Slider {
                    id: hueSlider
                    width: 290
                    from: 0
                    to: 1
                    value: colorPicker.hue
                    onMoved: colorPicker.hue = value
                }
            }
            Row {
                spacing: 10
                Text {
                    text: "饱和"
                    color: root.fgDim
                    font.pixelSize: 13
                    width: 40
                    anchors.verticalCenter: parent.verticalCenter
                }
                Slider {
                    id: satSlider
                    width: 290
                    from: 0
                    to: 1
                    value: colorPicker.sat
                    onMoved: colorPicker.sat = value
                }
            }
            Row {
                spacing: 10
                Text {
                    text: "亮度"
                    color: root.fgDim
                    font.pixelSize: 13
                    width: 40
                    anchors.verticalCenter: parent.verticalCenter
                }
                Slider {
                    id: valSlider
                    width: 290
                    from: 0
                    to: 1
                    value: colorPicker.val
                    onMoved: colorPicker.val = value
                }
            }
            Row {
                spacing: 12
                anchors.right: parent.right
                Rectangle {
                    width: 90
                    height: 36
                    radius: 18
                    color: root.rowBg
                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        color: root.fg
                        font.pixelSize: 14
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: colorPicker.close()
                    }
                }
                Rectangle {
                    width: 90
                    height: 36
                    radius: 18
                    color: root.accent
                    Text {
                        anchors.centerIn: parent
                        text: "确定"
                        color: "white"
                        font.pixelSize: 14
                        font.bold: true
                        font.family: root.uiFont
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var hex = Qt.hsva(colorPicker.hue, colorPicker.sat, colorPicker.val, 1).toString().toUpperCase()
                            if (root.pickerTarget === "card")
                                Settings.cardColor = hex
                            else if (root.pickerTarget === "font")
                                Settings.fontColor = hex
                            else
                                Settings.miniCardColor = hex
                            colorPicker.close()
                        }
                    }
                }
            }
        }
    }
}
