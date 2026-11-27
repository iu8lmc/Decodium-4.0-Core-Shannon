#include "src/app/AppImageQmlSnapshot.hpp"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <array>
#include <thread>

namespace {
bool writeFile(QString const& path, QByteArray const& data)
{
    if (!QDir().mkpath(QFileInfo(path).path())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data)==data.size();
}
bool populate(QString const& root, QByteArray const& main)
{
    return QDir().mkpath(root+"/dialogs") && QDir().mkpath(root+"/panels")
        && writeFile(root+"/decodium/BootLoader.qml", "import QtQuick\nQtObject {}\n")
        && writeFile(root+"/decodium/Main.qml", main)
        && writeFile(root+"/panels/Test.qml", "import QtQuick\nItem {}\n");
}
}

class TestAppImageQmlSnapshot : public QObject {
    Q_OBJECT
private slots:
    void stableAndVersioned()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QString a=temp.path()+"/mount-a", b=temp.path()+"/mount-b", cache=temp.path()+"/cache";
        QVERIFY(populate(a,"first")); QVERIFY(populate(b,"first"));
        QVERIFY(writeFile(a+"/decodium/Main.qmlc","stale compiled data"));
        QCOMPARE(decodium::appimage_qml::treeDigest(a,"qt-abi"),
                 QByteArray("5b12d82821c5c0d823cdf72ca0cc917fe71c0170938940c76a3b16eee410a20a"));
        QString const one=decodium::appimage_qml::prepare(a,cache,"qt-abi");
        QVERIFY(!one.isEmpty());
        QVERIFY(!QFileInfo::exists(one+"/decodium/Main.qmlc"));
        QCOMPARE(decodium::appimage_qml::prepare(b,cache,"qt-abi"),one);
        QVERIFY(writeFile(a+"/panels/OnlyCompiled.qmlc","no source"));
        QVERIFY(decodium::appimage_qml::prepare(a,cache,"qt-abi").isEmpty());
        QVERIFY(QFile::remove(a+"/panels/OnlyCompiled.qmlc"));
        QVERIFY(writeFile(one+"/decodium/Main.qmlc","unexpected compiled data"));
        QVERIFY(decodium::appimage_qml::prepare(a,cache,"qt-abi").isEmpty());
        QVERIFY(QFile::remove(one+"/decodium/Main.qmlc"));
        QVERIFY(QDir().mkpath(one+"/QtQuick"));
        QVERIFY(decodium::appimage_qml::prepare(a,cache,"qt-abi").isEmpty());
        QVERIFY(QDir().rmdir(one+"/QtQuick"));
        QDateTime const originalTime=QFileInfo(b+"/decodium/Main.qml").lastModified();
        QVERIFY(writeFile(b+"/decodium/Main.qml","other"));
        QFile changed(b+"/decodium/Main.qml");
        QVERIFY(changed.open(QIODevice::ReadWrite));
        QVERIFY(changed.setFileTime(originalTime,QFileDevice::FileModificationTime));
        changed.close();
        QString const two=decodium::appimage_qml::prepare(b,cache,"qt-abi");
        QVERIFY(!two.isEmpty()); QVERIFY(two!=one);
        QFile old(one+"/decodium/Main.qml"); QVERIFY(old.open(QIODevice::ReadOnly)); QCOMPARE(old.readAll(),QByteArray("first"));
        QFile updated(two+"/decodium/Main.qml"); QVERIFY(updated.open(QIODevice::ReadOnly)); QCOMPARE(updated.readAll(),QByteArray("other"));
        QVERIFY(decodium::appimage_qml::prepare(a,cache,"other-qt")!=one);
        QVERIFY(writeFile(one+"/decodium/Main.qml","corrupt"));
        QVERIFY(decodium::appimage_qml::prepare(a,cache,"qt-abi").isEmpty());
    }
    void concurrentPublication()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QString source=temp.path()+"/mount", cache=temp.path()+"/cache";
        QVERIFY(populate(source,"concurrent"));
        std::array<QString,8> paths;
        std::array<std::thread,8> workers;
        for (size_t i=0;i<workers.size();++i)
            workers[i]=std::thread([&,i] { paths[i]=decodium::appimage_qml::prepare(source,cache,"qt-abi"); });
        for (auto& worker:workers) worker.join();
        QVERIFY(!paths[0].isEmpty());
        for (auto const& path:paths) QCOMPARE(path,paths[0]);
        QCOMPARE(decodium::appimage_qml::treeDigest(paths[0],"qt-abi"),decodium::appimage_qml::treeDigest(source,"qt-abi"));
    }
    void rejectInvalidSource()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QString source=temp.path()+"/mount", cache=temp.path()+"/cache";
        QVERIFY(decodium::appimage_qml::prepare(source,cache,"salt").isEmpty());
        QVERIFY(populate(source,"main"));
        QVERIFY(writeFile(source+"/panels/plugin.so","native"));
        QVERIFY(decodium::appimage_qml::prepare(source,cache,"salt").isEmpty());
        QVERIFY(QFile::remove(source+"/panels/plugin.so"));
#if defined(Q_OS_UNIX)
        QVERIFY(QFile::link(source+"/decodium/Main.qml",source+"/panels/link.qml"));
        QVERIFY(decodium::appimage_qml::prepare(source,cache,"salt").isEmpty());
#endif
    }
};
QTEST_GUILESS_MAIN(TestAppImageQmlSnapshot)
#include "test_appimage_qml_snapshot.moc"
