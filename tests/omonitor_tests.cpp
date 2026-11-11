#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class OmonitorTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesHyprlandMonitors()
    {
        Backend backend(nullptr, {}, {}, false);
        const QByteArray json = R"([
          {"name":"eDP-1","description":"Laptop","width":1920,"height":1080,
           "refreshRate":60.0,"x":0,"y":0,"scale":1.25,"transform":0,
           "focused":true,"disabled":false,"availableModes":["1920x1080@60.00Hz"]},
          {"name":"DP-1","description":"Desk","width":2560,"height":1440,
           "refreshRate":143.97,"x":1536,"y":0,"scale":1.0,"transform":1,
           "focused":false,"disabled":false,"availableModes":["2560x1440@143.97Hz"]}
        ])";

        QVERIFY(backend.loadFromJson(json));
        QCOMPARE(backend.monitors().size(), 2);
        QCOMPARE(backend.monitors().at(0).toMap().value("name").toString(), QString("eDP-1"));
        QCOMPARE(backend.monitors().at(0).toMap().value("scale").toDouble(), 1.25);
        QCOMPARE(backend.monitors().at(0).toMap().value("modes").toStringList().first(),
                 QString("1920x1080@60.00"));
        QCOMPARE(backend.monitors().at(1).toMap().value("transform").toInt(), 1);
    }

    void updatesLayoutValues()
    {
        Backend backend(nullptr, {}, {}, false);
        QVERIFY(backend.loadFromJson(R"([{"name":"DP-1","width":1920,"height":1080,
          "refreshRate":60,"x":0,"y":0,"scale":1,"transform":0,"availableModes":[]}])"));
        backend.setPosition(0, -1920, 120);
        backend.setScale(0, 1.5);
        backend.setTransform(0, 3);
        const QVariantMap monitor = backend.monitors().first().toMap();
        QCOMPARE(monitor.value("x").toInt(), -1920);
        QCOMPARE(monitor.value("y").toInt(), 120);
        QCOMPARE(monitor.value("scale").toDouble(), 1.5);
        QCOMPARE(monitor.value("transform").toInt(), 3);
    }

    void writesAStableManagedBlock()
    {
        QVariantMap monitor{{"name", "DP-1"}, {"mode", "2560x1440@143.97Hz"},
                            {"x", 1920}, {"y", -120}, {"scale", 1.25}, {"transform", 1}};
        const QString block = Backend::managedBlock({monitor});
        QVERIFY(block.contains("-- omonitor:start"));
        QVERIFY(block.contains("output = \"DP-1\""));
        QVERIFY(block.contains("position = \"1920x-120\""));
        QVERIFY(block.contains("scale = 1.25"));
        QVERIFY(block.contains("transform = 1"));
    }

    void preservesManualConfiguration()
    {
        const QString original = "-- user setting\nhl.env(\"GDK_SCALE\", \"1\")\n\n"
                                 "-- omonitor:start\nold\n-- omonitor:end\n";
        const QString replaced = Backend::replaceManagedBlock(
            original, "-- omonitor:start\nnew\n-- omonitor:end\n");
        QVERIFY(replaced.contains("-- user setting"));
        QVERIFY(replaced.contains("hl.env"));
        QVERIFY(replaced.contains("new"));
        QVERIFY(!replaced.contains("old"));
        QCOMPARE(replaced.count("-- omonitor:start"), 1);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void translatorFollowsItalian()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "it\n");
        write(dir.path() + "/it.json", R"({"Apply":"Applica"})");
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("it"));
        QCOMPARE(translator.translate("Main", "Apply"), QString("Applica"));
        QVERIFY(translator.translate("Main", "Missing").isNull());
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), contents.size());
    }
};

QTEST_MAIN(OmonitorTests)
#include "omonitor_tests.moc"
