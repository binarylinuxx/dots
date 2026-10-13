#pragma once

#include <QObject>
#include <QVariantList>
#include <QTimer>
#include <QSocketNotifier>
#include <QHash>
#include <QSet>
#include <QtQml/qqmlregistration.h>
#include <wayland-client.h>

struct ext_zwwm_manager_v1;
struct ext_zwwm_subscription_v1;
struct zwwm_tags_v1;
struct zwwm_window_manager_v1;

class Zwwm : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(QVariantList tags READ tags NOTIFY stateChanged)
    Q_PROPERTY(QVariantList activeTags READ activeTags NOTIFY stateChanged)
    Q_PROPERTY(QVariantList clients READ clients NOTIFY stateChanged)
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY stateChanged)
    Q_PROPERTY(QString keyboardLayout READ keyboardLayout NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(bool canActivate READ canActivate NOTIFY stateChanged)
public:
    explicit Zwwm(QObject* parent = nullptr);
    ~Zwwm() override;
    bool available() const { return m_manager; }
    bool ready() const { return m_ready; }
    bool canActivate() const { return m_windowManager; }
    QVariantList tags() const { return m_tags; }
    QVariantList activeTags() const { return m_activeTags; }
    QVariantList clients() const { return m_clients; }
    QVariantList monitors() const { return m_monitors; }
    QString keyboardLayout() const { return m_keyboardLayout; }
    QString lastError() const { return m_error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setActiveTags(const QVariantList& ids);
    Q_INVOKABLE bool activateClient(const QString& id);
    Q_INVOKABLE bool dispatch(const QString& action, const QString& argument = {});
signals:
    void stateChanged();
    void commandFailed(const QString& message);
private:
    struct Snapshot {
        Zwwm* owner;
        uint32_t type;
        QVariantList clients, monitors;
        QHash<QString, uint32_t> outputTags;
        QString keyboard;
    };
    void connectDisplay();
    void disconnectDisplay();
    void flush();
    void failConnection(const QString& message);
    void reportError(const QString& message);
    void requestSnapshots();
    void requestSnapshot(uint32_t type);
    void publish(Snapshot* snapshot);
    void finishSnapshot(wl_proxy* proxy);
    void track(wl_proxy* proxy) { m_children.insert(proxy); }
    void untrack(wl_proxy* proxy) { m_children.remove(proxy); }
    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    ext_zwwm_manager_v1* m_manager = nullptr;
    ext_zwwm_subscription_v1* m_subscription = nullptr;
    zwwm_tags_v1* m_tagManager = nullptr;
    zwwm_window_manager_v1* m_windowManager = nullptr;
    uint32_t m_managerName = 0, m_tagName = 0, m_windowName = 0;
    QSocketNotifier* m_read = nullptr;
    QSocketNotifier* m_write = nullptr;
    QTimer m_refresh, m_retry;
    QSet<wl_proxy*> m_children;
    QHash<wl_proxy*, Snapshot*> m_snapshots;
    QHash<uint32_t, QString> m_pendingTags;
    QVariantList m_tags, m_activeTags, m_clients, m_monitors;
    QHash<QString, uint32_t> m_outputTags;
    QString m_keyboardLayout, m_error;
    bool m_ready = false, m_dirty = false, m_failing = false, m_stateDirty = false, m_batchFailed = false;
};
