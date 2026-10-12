import QtQuick

// 桌面上的单个卡片窗口（每张卡一个窗口，各自挂桌面层）。
// 上下文属性 Card 是本窗口对应的 CardBackend。
CardSlot {
    card: Card
}
