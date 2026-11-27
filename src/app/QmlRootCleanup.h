#pragma once

#include <QQmlApplicationEngine>
#include <QJSValue>
#include <QPointer>

// Destroy windows while the QML engine and its WorkerScript infrastructure
// are still alive. Otherwise the engine can destroy its worker engine first.
inline void destroyQmlRoots(QQmlApplicationEngine& engine)
{
    const auto roots = engine.rootObjects();
    for (QObject* root : roots) {
        // BootLoader creates Main.qml with createObject(null), so the main
        // window is not a child or an engine root. Destroy it first.
        const QVariant value = root->property("mainWindowObject");
        QObject* mainWindow = value.metaType() == QMetaType::fromType<QJSValue>()
            ? value.value<QJSValue>().toQObject() : value.value<QObject*>();
        QPointer<QObject> guardedRoot(root);
        if (mainWindow && mainWindow != root)
            delete mainWindow;
        if (!guardedRoot)
            continue;
        delete root;
    }
}
