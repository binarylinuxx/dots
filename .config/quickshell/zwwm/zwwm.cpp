#include "zwwm.hpp"
#include "ext-zwwm-manager-v1-client-protocol.h"
#include "zwwm-tags-unstable-v1-client-protocol.h"
#include "zwwm-window-manager-v1-client-protocol.h"

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace {
QString idString(uint32_t hi, uint32_t lo) {
    // QML numbers cannot represent all uint64 IDs exactly.
    return QString::number((uint64_t(hi) << 32) | lo);
}
template<typename T> wl_proxy* proxy(T* object) {
    return reinterpret_cast<wl_proxy*>(object);
}
QVariantMap client(uint32_t hi, uint32_t lo, const char* app, const char* title,
                   uint32_t outputHi, uint32_t outputLo, bool focused, uint32_t tag = 0,
                   bool fullscreen = false, bool hidden = false) {
    return {{"address", idString(hi, lo)}, {"className", QString::fromUtf8(app)},
            {"title", QString::fromUtf8(title)}, {"output", idString(outputHi, outputLo)},
            {"activated", focused}, {"workspace", QVariantMap{{"id", tag}}},
            {"fullscreen", fullscreen}, {"hidden", hidden}};
}
}

Zwwm::Zwwm(QObject* parent) : QObject(parent) {
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(30);
    connect(&m_refresh, &QTimer::timeout, this, &Zwwm::requestSnapshots);
    m_retry.setSingleShot(true);
    m_retry.setInterval(2000);
    connect(&m_retry, &QTimer::timeout, this, &Zwwm::connectDisplay);
    QTimer::singleShot(0, this, &Zwwm::connectDisplay);
}

Zwwm::~Zwwm() { disconnectDisplay(); }

void Zwwm::connectDisplay() {
    if (m_display || qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) return;
    m_display = wl_display_connect(nullptr);
    if (!m_display) { m_retry.start(); return; }
    m_failing = false;
    m_error.clear();
    m_registry = wl_display_get_registry(m_display);
    static const wl_registry_listener registryListener = {
        [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
            auto* self = static_cast<Zwwm*>(data);
            if (!std::strcmp(interface, "ext_zwwm_manager_v1") && !self->m_manager) {
                self->m_managerName = name;
                self->m_manager = static_cast<ext_zwwm_manager_v1*>(wl_registry_bind(registry, name, &ext_zwwm_manager_v1_interface, std::min(version, 6u)));
                const uint32_t mask = version >= 6 ? 511u : 127u;
                self->m_subscription = ext_zwwm_manager_v1_subscribe(self->m_manager, mask);
                static const ext_zwwm_subscription_v1_listener listener = {
                    [](void* data, ext_zwwm_subscription_v1*, const char*) { static_cast<Zwwm*>(data)->refresh(); }
                };
                ext_zwwm_subscription_v1_add_listener(self->m_subscription, &listener, self);
                self->refresh();
                emit self->stateChanged();
            } else if (!std::strcmp(interface, "zwwm_tags_v1") && !self->m_tagManager) {
                self->m_tagName = name;
                self->m_tagManager = static_cast<zwwm_tags_v1*>(wl_registry_bind(registry, name, &zwwm_tags_v1_interface, 1));
                static const zwwm_tags_v1_listener listener = {
                    [](void* data, zwwm_tags_v1*, uint32_t id, const char* name) { static_cast<Zwwm*>(data)->m_pendingTags[id] = QString::fromUtf8(name); },
                    [](void* data, zwwm_tags_v1*, uint32_t id) { static_cast<Zwwm*>(data)->m_pendingTags.remove(id); },
                    [](void* data, zwwm_tags_v1*) {
                        auto* self = static_cast<Zwwm*>(data);
                        auto ids = self->m_pendingTags.keys();
                        std::sort(ids.begin(), ids.end());
                        self->m_tags.clear();
                        for (auto id : ids) self->m_tags.append(QVariantMap{{"id", id}, {"name", self->m_pendingTags[id]}});
                        emit self->stateChanged();
                    },
                    [](void* data, zwwm_tags_v1*, wl_array* array) {
                        auto* self = static_cast<Zwwm*>(data);
                        self->m_activeTags.clear();
                        const auto* bytes = static_cast<const char*>(array->data);
                        for (size_t offset = 0; offset + sizeof(uint32_t) <= array->size; offset += sizeof(uint32_t)) {
                            uint32_t id;
                            std::memcpy(&id, bytes + offset, sizeof(id));
                            self->m_activeTags.append(id);
                        }
                        emit self->stateChanged();
                    },
                    [](void*, zwwm_tags_v1*, xdg_toplevel*, wl_array*) {},
                    [](void*, zwwm_tags_v1*, xdg_toplevel*) {}
                };
                zwwm_tags_v1_add_listener(self->m_tagManager, &listener, self);
            } else if (!std::strcmp(interface, "zwwm_window_manager_v1") && version >= 2 && !self->m_windowManager) {
                self->m_windowName = name;
                self->m_windowManager = static_cast<zwwm_window_manager_v1*>(wl_registry_bind(registry, name, &zwwm_window_manager_v1_interface, 2));
                static const zwwm_window_manager_v1_listener listener = {
                    [](void*, zwwm_window_manager_v1*, uint32_t) {},
                    [](void* data, zwwm_window_manager_v1*, uint32_t, uint32_t) { static_cast<Zwwm*>(data)->refresh(); },
                    [](void*, zwwm_window_manager_v1*, uint32_t, uint32_t, uint32_t, int32_t, int32_t) {},
                    [](void*, zwwm_window_manager_v1*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
                    [](void*, zwwm_window_manager_v1*, const char*, uint32_t, uint32_t, uint32_t) {},
                    [](void*, zwwm_window_manager_v1*, uint32_t, int32_t, uint32_t, int32_t, int32_t) {},
                    [](void*, zwwm_window_manager_v1*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t, uint32_t, const char*, const char*) {},
                    [](void*, zwwm_window_manager_v1*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
                };
                zwwm_window_manager_v1_add_listener(self->m_windowManager, &listener, self);
                self->refresh();
            }
        },
        [](void* data, wl_registry*, uint32_t name) {
            auto* self = static_cast<Zwwm*>(data);
            if (name == self->m_managerName || name == self->m_tagName || name == self->m_windowName)
                self->failConnection(QStringLiteral("zwwm protocol global removed"));
        }
    };
    wl_registry_add_listener(m_registry, &registryListener, this);
    m_read = new QSocketNotifier(wl_display_get_fd(m_display), QSocketNotifier::Read, this);
    m_write = new QSocketNotifier(wl_display_get_fd(m_display), QSocketNotifier::Write, this);
    m_write->setEnabled(false);
    connect(m_read, &QSocketNotifier::activated, this, [this] {
        if (m_failing) return;
        if (wl_display_dispatch(m_display) < 0) failConnection(QStringLiteral("zwwm Wayland connection closed"));
        else flush();
    });
    connect(m_write, &QSocketNotifier::activated, this, [this] { flush(); });
    flush();
}

void Zwwm::disconnectDisplay() {
    m_refresh.stop();
    delete m_read; m_read = nullptr;
    delete m_write; m_write = nullptr;
    for (auto* snapshot : std::as_const(m_snapshots)) delete snapshot;
    m_snapshots.clear();
    for (auto* child : std::as_const(m_children)) wl_proxy_destroy(child);
    m_children.clear();
    if (m_subscription) wl_proxy_destroy(proxy(m_subscription));
    if (m_windowManager) wl_proxy_destroy(proxy(m_windowManager));
    if (m_tagManager) wl_proxy_destroy(proxy(m_tagManager));
    if (m_manager) wl_proxy_destroy(proxy(m_manager));
    if (m_registry) wl_registry_destroy(m_registry);
    if (m_display) wl_display_disconnect(m_display);
    m_display = nullptr; m_registry = nullptr; m_manager = nullptr;
    m_subscription = nullptr; m_windowManager = nullptr; m_tagManager = nullptr;
    m_managerName = m_tagName = m_windowName = 0;
    m_ready = m_dirty = m_stateDirty = m_batchFailed = false;
    m_tags.clear(); m_pendingTags.clear(); m_activeTags.clear();
    m_clients.clear(); m_monitors.clear(); m_outputTags.clear(); m_keyboardLayout.clear();
}

void Zwwm::failConnection(const QString& message) {
    if (m_failing) return;
    m_failing = true;
    if (m_read) m_read->setEnabled(false);
    if (m_write) m_write->setEnabled(false);
    // Never delete callback data or the display inside a Wayland listener.
    QTimer::singleShot(0, this, [this, message] {
        disconnectDisplay();
        reportError(message);
        m_retry.start();
    });
}

void Zwwm::flush() {
    if (!m_display || m_failing) return;
    if (wl_display_flush(m_display) >= 0) m_write->setEnabled(false);
    else if (errno == EAGAIN) m_write->setEnabled(true);
    else failConnection(QStringLiteral("could not flush zwwm Wayland requests"));
}

void Zwwm::reportError(const QString& message) {
    m_error = message;
    emit commandFailed(message);
    emit stateChanged();
}

void Zwwm::refresh() {
    m_dirty = true;
    if (!m_refresh.isActive()) m_refresh.start();
}

void Zwwm::requestSnapshots() {
    if (!m_manager || m_failing || !m_snapshots.isEmpty()) return;
    m_dirty = false;
    m_batchFailed = false;
    if (m_windowManager) {
        auto* object = zwwm_window_manager_v1_get_state(m_windowManager);
        auto* snapshot = new Snapshot{this, 100, {}, {}, {}, {}};
        track(proxy(object)); m_snapshots.insert(proxy(object), snapshot);
        static const zwwm_window_state_v1_listener listener = {
            [](void* data, zwwm_window_state_v1*, uint32_t hi, uint32_t lo, const char* name, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t tag) {
                auto* s = static_cast<Snapshot*>(data);
                s->monitors.append(QVariantMap{{"id", idString(hi, lo)}, {"name", QString::fromUtf8(name)}, {"x", x}, {"y", y}, {"width", w}, {"height", h}, {"activeWorkspace", QVariantMap{{"id", tag}}}});
                s->outputTags[idString(hi, lo)] = tag;
            },
            [](void* data, zwwm_window_state_v1*, uint32_t hi, uint32_t lo, uint32_t oh, uint32_t ol, uint32_t tag, uint32_t state, const char* app, const char* title, uint32_t, uint32_t, int32_t, int32_t, uint32_t, uint32_t, int32_t, int32_t, int32_t, int32_t) {
                if (state & ZWWM_WINDOW_STATE_V1_WINDOW_STATE_MAPPED)
                    static_cast<Snapshot*>(data)->clients.append(client(hi, lo, app, title, oh, ol,
                        state & ZWWM_WINDOW_STATE_V1_WINDOW_STATE_FOCUSED, tag,
                        state & ZWWM_WINDOW_STATE_V1_WINDOW_STATE_FULLSCREEN,
                        state & ZWWM_WINDOW_STATE_V1_WINDOW_STATE_HIDDEN));
            },
            [](void*, zwwm_window_state_v1*, uint32_t) {},
            [](void* data, zwwm_window_state_v1* object, uint32_t) {
                auto* s = static_cast<Snapshot*>(data);
                auto* owner = s->owner;
                owner->publish(s);
                owner->finishSnapshot(proxy(object));
                zwwm_window_state_v1_destroy(object);
            }
        };
        zwwm_window_state_v1_add_listener(object, &listener, snapshot);
    } else {
        requestSnapshot(EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_OUTPUTS);
        requestSnapshot(EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_TOPLEVELS);
        requestSnapshot(EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_TAGS);
    }
    requestSnapshot(EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_KEYBOARD);
    flush();
}

void Zwwm::requestSnapshot(uint32_t type) {
    auto* object = ext_zwwm_manager_v1_get_snapshot(m_manager, type);
    auto* snapshot = new Snapshot{this, type, {}, {}, {}, {}};
    track(proxy(object)); m_snapshots.insert(proxy(object), snapshot);
    static const ext_zwwm_snapshot_v1_listener listener = {
        [](void*, ext_zwwm_snapshot_v1*, uint32_t, uint32_t, uint32_t, uint32_t) {},
        [](void* data, ext_zwwm_snapshot_v1*, const char* name, uint32_t hi, uint32_t lo, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t, uint32_t, uint32_t refresh, uint32_t scale, uint32_t, uint32_t, uint32_t enabled, uint32_t) {
            static_cast<Snapshot*>(data)->monitors.append(QVariantMap{{"id", idString(hi, lo)}, {"name", QString::fromUtf8(name)}, {"x", x}, {"y", y}, {"width", w}, {"height", h}, {"refreshRate", refresh / 1000.0}, {"scale", scale / 1000.0}, {"enabled", bool(enabled)}});
        },
        [](void*, ext_zwwm_snapshot_v1*, uint32_t, uint32_t, uint32_t, uint32_t) {},
        [](void* data, ext_zwwm_snapshot_v1*, uint32_t hi, uint32_t lo, const char* app, const char* title, uint32_t oh, uint32_t ol, int32_t, int32_t, int32_t, int32_t, uint32_t state) {
            static_cast<Snapshot*>(data)->clients.append(client(hi, lo, app, title, oh, ol, state & 4u));
        },
        [](void* data, ext_zwwm_snapshot_v1*, const char*, uint32_t hi, uint32_t lo, uint32_t active) { static_cast<Snapshot*>(data)->outputTags[idString(hi, lo)] = active; },
        [](void*, ext_zwwm_snapshot_v1*, uint32_t, uint32_t, uint32_t, uint32_t, const char*, uint32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t) {},
        [](void*, ext_zwwm_snapshot_v1*, const char*, uint32_t, uint32_t, uint32_t, const char*, const char*, const char*, uint32_t) {},
        [](void* data, ext_zwwm_snapshot_v1*, const char* layout, uint32_t) { static_cast<Snapshot*>(data)->keyboard = QString::fromUtf8(layout); },
        [](void* data, ext_zwwm_snapshot_v1* object) {
            auto* s = static_cast<Snapshot*>(data);
            auto* owner = s->owner;
            owner->publish(s);
            owner->finishSnapshot(proxy(object));
            ext_zwwm_snapshot_v1_destroy(object);
        },
        [](void* data, ext_zwwm_snapshot_v1* object, const char* message) {
            auto* owner = static_cast<Snapshot*>(data)->owner;
            owner->m_batchFailed = true;
            owner->reportError(QString::fromUtf8(message));
            owner->finishSnapshot(proxy(object));
            ext_zwwm_snapshot_v1_destroy(object);
        }
    };
    ext_zwwm_snapshot_v1_add_listener(object, &listener, snapshot);
}

void Zwwm::publish(Snapshot* s) {
    const auto oldClients = m_clients, oldMonitors = m_monitors;
    const auto oldKeyboard = m_keyboardLayout;
    if (s->type == 100 || s->type == EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_TOPLEVELS) m_clients = s->clients;
    if (s->type == 100 || s->type == EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_OUTPUTS) m_monitors = s->monitors;
    if (s->type == 100 || s->type == EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_TAGS) m_outputTags = s->outputTags;
    if (s->type == EXT_ZWWM_MANAGER_V1_SNAPSHOT_TYPE_KEYBOARD) m_keyboardLayout = s->keyboard;
    for (auto& monitor : m_monitors) {
        auto map = monitor.toMap();
        map["activeWorkspace"] = QVariantMap{{"id", m_outputTags.value(map["id"].toString())}};
        monitor = map;
    }
    // The ext manager does not expose window membership; preserve unknown (0).
    m_stateDirty = m_stateDirty || oldClients != m_clients || oldMonitors != m_monitors || oldKeyboard != m_keyboardLayout;
}

void Zwwm::finishSnapshot(wl_proxy* object) {
    delete m_snapshots.take(object);
    untrack(object);
    if (m_snapshots.isEmpty()) {
        const bool changed = m_stateDirty || m_ready != !m_batchFailed;
        m_ready = !m_batchFailed;
        m_stateDirty = false;
        if (changed) emit stateChanged();
    }
    if (m_snapshots.isEmpty() && m_dirty) m_refresh.start();
}

bool Zwwm::setActiveTags(const QVariantList& ids) {
    if (!m_tagManager || m_failing) return false;
    // This compositor implements one active tag; validate before sending to
    // avoid a fatal protocol error for invalid IDs or multiple selections.
    if (ids.size() > 1) { reportError(QStringLiteral("zwwm supports one active tag")); return false; }
    uint32_t id = 0;
    if (!ids.isEmpty()) {
        bool ok = false;
        const auto value = ids.first().toULongLong(&ok);
        if (!ok || value > UINT32_MAX || !m_pendingTags.contains(uint32_t(value))) {
            reportError(QStringLiteral("unknown zwwm tag")); return false;
        }
        id = uint32_t(value);
    }
    wl_array array{ids.isEmpty() ? size_t(0) : sizeof(id), sizeof(id), &id};
    zwwm_tags_v1_set_active_tags(m_tagManager, &array);
    flush();
    return true;
}

bool Zwwm::activateClient(const QString& id) {
    if (!m_windowManager || m_failing) { reportError(QStringLiteral("window activation requires zwwm-window-manager-v1 version 2")); return false; }
    bool ok;
    const uint64_t value = id.toULongLong(&ok);
    if (!ok || !value) return false;
    auto* object = zwwm_window_manager_v1_window_action(m_windowManager, uint32_t(value >> 32), uint32_t(value), ZWWM_WINDOW_MANAGER_V1_WINDOW_ACTION_FOCUS, 1);
    track(proxy(object));
    static const zwwm_window_result_v1_listener listener = {
        [](void* data, zwwm_window_result_v1* object) {
            auto* self = static_cast<Zwwm*>(data);
            self->untrack(proxy(object)); zwwm_window_result_v1_destroy(object); self->refresh();
        },
        [](void* data, zwwm_window_result_v1* object, const char* message) {
            auto* self = static_cast<Zwwm*>(data);
            self->reportError(QString::fromUtf8(message));
            self->untrack(proxy(object)); zwwm_window_result_v1_destroy(object);
        }
    };
    zwwm_window_result_v1_add_listener(object, &listener, this);
    flush();
    return true;
}

bool Zwwm::dispatch(const QString& action, const QString& argument) {
    if (!m_manager || m_failing) return false;
    const auto a = action.toUtf8(), b = argument.toUtf8();
    auto* object = ext_zwwm_manager_v1_dispatch(m_manager, a.constData(), b.constData());
    track(proxy(object));
    static const ext_zwwm_result_v1_listener listener = {
        [](void* data, ext_zwwm_result_v1* object, const char*) {
            auto* self = static_cast<Zwwm*>(data);
            self->untrack(proxy(object)); ext_zwwm_result_v1_destroy(object); self->refresh();
        },
        [](void* data, ext_zwwm_result_v1* object, const char* message) {
            auto* self = static_cast<Zwwm*>(data);
            self->reportError(QString::fromUtf8(message));
            self->untrack(proxy(object)); ext_zwwm_result_v1_destroy(object);
        }
    };
    ext_zwwm_result_v1_add_listener(object, &listener, this);
    flush();
    return true;
}
