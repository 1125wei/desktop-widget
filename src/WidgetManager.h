#pragma once

#include <QList>
#include <QObject>
#include <QTimer>

#include "DesktopLayer.h"

class CardBackend;
class ConfigStore;
class QQuickView;

// 多卡片管理器：**每张卡一个窗口**（各自挂桌面层、各自摆位、各自配置）。
//
// 为什么不是"一个大窗口里画多张卡"：
//   · 桌面层挂载/脱层/失效重挂这套 Win32 逻辑已经验证过，按窗口复用 = 零新风险
//   · 每张卡独立：位置互不影响，单卡出问题不连坐
//   · 编辑模式可以直接复用现有的"窗口级拖动"（Win32 物理像素，150% 缩放下也对）
class WidgetManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    Q_PROPERTY(int cardCount READ cardCount NOTIFY cardsChanged)

public:
    WidgetManager(ConfigStore* cfg, DesktopLayer::Mode mode, QObject* parent = nullptr);
    ~WidgetManager() override;

    void start(); // 按配置建出所有卡片窗口 → 挂桌面层 → 恢复位置

    bool editing() const { return m_editing; }
    int cardCount() const { return m_cards.size(); }
    bool visible() const { return m_visible; }
    DesktopLayer::Mode mode() const { return m_mode; }
    QList<CardBackend*> cards() const { return m_cards; }
    CardBackend* cardAt(int i) const { return (i >= 0 && i < m_cards.size()) ? m_cards.at(i) : nullptr; }
    QQuickView* viewAt(int i) const { return (i >= 0 && i < m_views.size()) ? m_views.at(i) : nullptr; }

    // 编辑模式是全局的：所有卡片一起进入/退出（卡片上点"完成"、托盘勾选都走这里）
    void setEditing(bool on);

    void reloadAll();            // 配置改动 → 所有卡片重读（实时生效）
    void reloadCard(int index);
    void saveAllPositions();
    void resetAllPositions();
    void remountAll();
    void setVisibleAll(bool on);
    int addCard(const QString& type); // 追加一张卡，返回新索引（-1 = 失败）
    void removeCard(int index);
    void quitApp();
    void dumpState(const QString& tag) const;

signals:
    void editingChanged();
    void cardsChanged();

private:
    void createCard(int index);
    void restorePosition(int index, bool useConfig);
    void placeBelowPrevious(int index);
    void applyLayer(int index);
    void repaintCard(int index, const QString& tag);
    void pullBackToScreen(int index);

    ConfigStore* m_cfg = nullptr;
    DesktopLayer::Mode m_mode = DesktopLayer::Auto;
    QList<CardBackend*> m_cards;
    QList<QQuickView*> m_views;
    bool m_editing = false;
    bool m_visible = true;
    QTimer m_guard;
};
