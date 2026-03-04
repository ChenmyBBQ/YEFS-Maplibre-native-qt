// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "export_quick_p.hpp"

#include <QMapLibre/Map>

#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGSimpleTextureNode>

#include <functional>

namespace QMapLibre {

// Base class for backend-specific texture nodes
class Q_MAPLIBRE_QUICKPRIVATE_EXPORT TextureNodeBase : public QSGSimpleTextureNode {
public:
    explicit TextureNodeBase(const Settings &settings, const QSize &size, qreal pixelRatio);
    explicit TextureNodeBase(std::shared_ptr<Map> map, const QSize &size, qreal pixelRatio);
    ~TextureNodeBase() override = default;

    [[nodiscard]] Map *map() const { return m_map.get(); }

    // 设置首帧回调：每当底层成功向 Qt SceneGraph 提交有效纹理时触发
    // 由渲染线程调用，回调实现需自行保证线程安全（如 QMetaObject::invokeMethod QueuedConnection）
    void setFirstFrameCallback(std::function<void()> cb) { m_firstFrameCallback = std::move(cb); }

    virtual void resize(const QSize &size, qreal pixelRatio, QQuickWindow *window) = 0;
    virtual void render(QQuickWindow *window) = 0;

protected:
    std::shared_ptr<Map> m_map;
    QSize m_size;
    qreal m_pixelRatio{1.0};
    std::function<void()> m_firstFrameCallback;
};

} // namespace QMapLibre
