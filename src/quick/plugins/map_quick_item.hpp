// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <QtCore/qtmetamacros.h>
#include <QMapLibre/Map>
#include <QMapLibre/Settings>
#include <atomic>

#include <QtQuick/QQuickItem>
#include <QtQuick/QSGNode>

#include <memory>
#include "types.hpp"

namespace QMapLibre {

class MapQuickItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(MapLibre)
    QML_ADDED_IN_VERSION(3, 0)

    Q_PROPERTY(QString style READ style WRITE setStyle)
    Q_PROPERTY(QVariantList coordinate READ coordinate WRITE setCoordinate NOTIFY coordinateChanged)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    Q_PROPERTY(double bearing READ bearing WRITE setBearing NOTIFY bearingChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY pitchChanged)

public:
    explicit MapQuickItem(QQuickItem *parent = nullptr);

    [[nodiscard]] QString style() const { return m_style; }
    void setStyle(const QString &style);

    [[nodiscard]] double zoomLevel() const { return m_zoomLevel; }
    void setZoomLevel(double zoomLevel);

    [[nodiscard]] double bearing() const { return m_bearing; }
    void setBearing(double bearing);

    [[nodiscard]] double pitch() const { return m_pitch; }
    void setPitch(double pitch);

    [[nodiscard]] QVariantList coordinate() const { return m_coordinate; }
    void setCoordinate(const QVariantList &coordinate);
    Q_INVOKABLE void setCoordinateFromPixel(const QPointF &pixel);
    Q_INVOKABLE QVariantList coordinateForPixel(const QPointF &pixel) const;

    Q_INVOKABLE void pan(const QPointF &offset);
    Q_INVOKABLE void scale(double scale, const QPointF &center);
    Q_INVOKABLE void easeTo(const QVariantMap &camera, const QVariantMap &animation = QVariantMap());
    Q_INVOKABLE void flyTo(const QVariantMap &camera, const QVariantMap &animation = QVariantMap());

    enum SyncState : int {
        NoSync = 0,
        ViewportSync = 1 << 0,
        CameraOptionsSync = 1 << 1,
    };
    Q_DECLARE_FLAGS(SyncStates, SyncState);

signals:
    void coordinateChanged();
    void zoomLevelChanged();
    void bearingChanged();
    void pitchChanged();
    void styleLoaded();    // 样式下载并解析完毕
    void mapFullyLoaded(); // MapLibre 渲染线程报告所有瓦片完成
    void firstFrameReady(); // Qt SceneGraph 已持有有效纹理，遮罩可安全撤除（根本解决闪屏）
    void mapLoadFailed();

protected:
    void componentComplete() override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

public slots:
    QObject* getNativeMap() const { return m_map.get(); }

private slots:
    void initialize();
    void onMapChanged(Map::MapChange change);

private:
    QSGNode *updateMapNode(QSGNode *node);

    Settings m_settings;
    std::shared_ptr<Map> m_map;

    SyncStates m_syncState = NoSync;
    QVariantList m_coordinate{0, 0};
    double m_zoomLevel{};
    double m_bearing{};
    double m_pitch{};
    QString m_style;

    // mapFullyLoaded 到达后置 true，下一次 render() 触发 firstFrameReady 后自动清零
    // 用 atomic 保证主线程写、渲染线程读的线程安全
    std::atomic<bool> m_awaitFirstFrameAfterLoad{false};

    // 每次样式变化（WillLoadMap/WillChangeStyle）时置 true，允许 arm 一次门控；
    // 首次 DidFinishRenderingMapFullyRendered 且 isFullyLoaded=true 后清零，
    // 防止后续重复触发 DidFinishRenderingMapFullyRendered 反复 arm 门控
    bool m_expectingFirstFrame{true};
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MapQuickItem::SyncStates)

} // namespace QMapLibre

