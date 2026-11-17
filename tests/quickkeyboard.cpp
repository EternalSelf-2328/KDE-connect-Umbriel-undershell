// SPDX-License-Identifier: GPL-3.0-or-later
#include "quickkeyboard.h"
#include <QApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
class QuickKeyboardTests : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() { installQuickKeyboard(qApp); }
    void movesBetweenVisibleControls() {
        QQuickWindow window;
        window.resize(360, 160);
        QQuickItem one(window.contentItem()), two(window.contentItem()), hidden(window.contentItem());
        one.setSize({80, 40});
        one.setPosition({20, 20});
        one.setActiveFocusOnTab(true);
        two.setSize({80, 40});
        two.setPosition({140, 20});
        two.setActiveFocusOnTab(true);
        hidden.setSize({80, 40});
        hidden.setPosition({100, 20});
        hidden.setActiveFocusOnTab(true);
        hidden.setVisible(false);
        window.show();
        window.requestActivate();
        one.forceActiveFocus();
        QTRY_VERIFY(window.isActive());
        QTest::keyClick(&window, Qt::Key_L);
        QCOMPARE(window.activeFocusItem(), &two);
        QTest::keyClick(&window, Qt::Key_J);
        QCOMPARE(window.activeFocusItem(), &one);
    }
    void preservesTextInput() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick; TextInput { width: 200; height: 40 }", {});
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        window.resize(300, 160);
        auto *input = qobject_cast<QQuickItem *>(component.create());
        QVERIFY(input);
        input->setParentItem(window.contentItem());
        window.show();
        window.requestActivate();
        input->forceActiveFocus();
        QTRY_VERIFY(window.isActive());
        for (auto key : {Qt::Key_I, Qt::Key_J, Qt::Key_K, Qt::Key_L, Qt::Key_Q})
            QTest::keyClick(&window, key);
        QCOMPARE(input->property("text").toString(), QString("ijklq"));
        QVERIFY(window.isVisible());
        delete input;
    }
};
QTEST_MAIN(QuickKeyboardTests)
#include "quickkeyboard.moc"
