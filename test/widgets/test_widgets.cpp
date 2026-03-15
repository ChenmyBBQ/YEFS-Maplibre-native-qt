// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include "main_window.hpp"
#include "map_widget_tester.hpp"
#include "map_window.hpp"

#include <QMapLibre/Map>

#include "style/style_expression.hpp"

#include <QDebug>
#include <QTest>

#include <memory>

class TestWidgets : public QObject {
    Q_OBJECT

private slots:
    void testGLWidgetNoProvider();
    void testGLWidgetMapLibreProvider();
    void testGLWidgetDocking();
    void testGLWidgetStyle();
    void testStyleExpressionBuilder();
    void testLayerStateStyle();
};

void TestWidgets::testGLWidgetNoProvider() {
    const QMapLibre::Settings settings;
    auto widget = std::make_unique<QMapLibre::MapWidget>(settings);
    widget->show();
    QTest::qWait(1000);
}

void TestWidgets::testGLWidgetMapLibreProvider() {
    QMapLibre::Settings settings(QMapLibre::Settings::MapLibreProvider);
    settings.setDefaultCoordinate(QMapLibre::Coordinate(59.91, 10.75));
    settings.setDefaultZoom(4);
    auto tester = std::make_unique<QMapLibre::Test::MapWidgetTester>(settings);
    tester->show();
    QTest::qWait(1000);
    qDebug() << "Resizing to" << QSize(800, 600);
    tester->resize(800, 600);
    QTest::qWait(1000);
    qDebug() << "Resizing to" << QSize(400, 300);
    tester->resize(400, 300);
    QTest::qWait(1000);
    qDebug() << "Hiding";
    tester->hide();
    QTest::qWait(500);
    qDebug() << "Showing";
    tester->show();
    QTest::qWait(1000);
}

void TestWidgets::testGLWidgetDocking() {
    QMapLibre::Settings settings(QMapLibre::Settings::MapLibreProvider);
    settings.setDefaultCoordinate(QMapLibre::Coordinate(59.91, 10.75));
    settings.setDefaultZoom(4);
    auto tester = std::make_unique<QMapLibre::Test::MainWindow>();
    tester->show();
    QTest::qWait(1000);
    qDebug() << "Undocking";
    QMapLibre::Test::MapWindow *window = tester->currentCentralWidget();
    window->dockUndock();
    QTest::qWait(500);
    qDebug() << "Resizing undocked window to" << QSize(800, 600);
    window->resize(800, 600);
    QTest::qWait(500);
    qDebug() << "Docking back";
    window->dockUndock();
    QTest::qWait(1000);
}

void TestWidgets::testGLWidgetStyle() {
    QMapLibre::Styles styles;
    styles.append(QMapLibre::Style("https://demotiles.maplibre.org/style.json", "Demo tiles"));

    QMapLibre::Settings settings;
    settings.setStyles(styles);
    auto tester = std::make_unique<QMapLibre::Test::MapWidgetTester>(settings);
    tester->show();
    QTest::qWait(100);
    tester->initializeAnimation();
    QTest::qWait(tester->selfTest());
}

void TestWidgets::testStyleExpressionBuilder() {
    const QVariantList stateExpr = QMapLibre::StyleExpression::booleanFeatureState(QStringLiteral("selected"));
    QCOMPARE(stateExpr.value(0).toString(), QStringLiteral("boolean"));

    const QVariantList featureStateExpr = stateExpr.value(1).toList();
    QCOMPARE(featureStateExpr.value(0).toString(), QStringLiteral("feature-state"));
    QCOMPARE(featureStateExpr.value(1).toString(), QStringLiteral("selected"));
    QCOMPARE(stateExpr.value(2).toBool(), false);

    const QVariantList caseExpr = QMapLibre::StyleExpression::caseWhen(
        QVariant::fromValue(QMapLibre::StyleExpression::eq(QVariant::fromValue(QMapLibre::StyleExpression::get(QStringLiteral("status"))),
                                                           QStringLiteral("alarm"))),
        QStringLiteral("red"),
        QStringLiteral("blue"));
    QCOMPARE(caseExpr.value(0).toString(), QStringLiteral("case"));
    QCOMPARE(caseExpr.value(2).toString(), QStringLiteral("red"));
    QCOMPARE(caseExpr.value(3).toString(), QStringLiteral("blue"));
}

void TestWidgets::testLayerStateStyle() {
    QMapLibre::Map map(nullptr, QMapLibre::Settings(), QSize(256, 256), 1);
    map.setStyleJson(QStringLiteral("{\"version\":8,\"sources\":{},\"layers\":[]}"));

    QVariantMap source;
    source.insert(QStringLiteral("type"), QStringLiteral("geojson"));
    source.insert(QStringLiteral("data"), QByteArray("{\"type\":\"FeatureCollection\",\"features\":[]}"));
    map.addSource(QStringLiteral("shape-source"), source);

    QVariantMap paint;
    paint.insert(QStringLiteral("fill-opacity"), 0.4);

    QVariantMap layer;
    layer.insert(QStringLiteral("type"), QStringLiteral("fill"));
    layer.insert(QStringLiteral("source"), QStringLiteral("shape-source"));
    layer.insert(QStringLiteral("paint"), paint);
    map.addLayer(QStringLiteral("shape-layer"), layer);

    QVariantMap statePaint;
    statePaint.insert(QStringLiteral("fill-opacity"), 0.9);
    QVariantMap statePatch;
    statePatch.insert(QStringLiteral("paint"), statePaint);

    QVERIFY(map.registerLayerStateStyle(QStringLiteral("shape-layer"), QStringLiteral("selected"), statePatch));
    QVERIFY(map.setLayerStateStyleActive(QStringLiteral("shape-layer"), QStringLiteral("selected"), true));
    QCOMPARE(map.activeLayerStateStyles(QStringLiteral("shape-layer")), QStringList{QStringLiteral("selected")});

    QVariantMap snapshot = map.stylePropertySnapshot(QStringLiteral("shape-layer"), {QStringLiteral("fill-opacity")});
    QCOMPARE(snapshot.value(QStringLiteral("fill-opacity")).toMap().value(QStringLiteral("value")).toDouble(), 0.9);

    QVERIFY(map.setLayerStateStyleActive(QStringLiteral("shape-layer"), QStringLiteral("selected"), false));
    snapshot = map.stylePropertySnapshot(QStringLiteral("shape-layer"), {QStringLiteral("fill-opacity")});
    QCOMPARE(snapshot.value(QStringLiteral("fill-opacity")).toMap().value(QStringLiteral("value")).toDouble(), 0.4);

    QVERIFY(!map.setLayerStateStyleActive(QStringLiteral("shape-layer"), QStringLiteral("missing"), true));
    QCOMPARE(map.lastStyleErrorCode(), QMapLibre::Map::StyleStateStyleNotFoundError);
}

// NOLINTNEXTLINE(misc-const-correctness)
QTEST_MAIN(TestWidgets)
#include "test_widgets.moc"
