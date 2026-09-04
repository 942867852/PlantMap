#include <QApplication>
#include <QCommandLineParser>
#include <QDir>

#include "mainwindow.h"

namespace {

QString defaultDataDir()
{
    const QByteArray env = qgetenv("PLANTMAP_DATA_DIR");
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env);

#ifdef PLANTMAP_SOURCE_DATA_DIR
    return QStringLiteral(PLANTMAP_SOURCE_DATA_DIR);
#else
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("data"));
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PlantMap"));
    QApplication::setOrganizationName(QStringLiteral("PlantMap"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("植物图谱 PlantMap"));
    parser.addHelpOption();
    parser.addOption({ QStringLiteral("data-dir"),
                       QStringLiteral("数据目录（默认：源码 data/ 或环境变量 PLANTMAP_DATA_DIR）"),
                       QStringLiteral("path") });
    parser.process(app);

    QString dataDir = parser.value(QStringLiteral("data-dir"));
    if (dataDir.isEmpty())
        dataDir = defaultDataDir();
    QDir().mkpath(dataDir);

    MainWindow window(dataDir);
    window.resize(1280, 800);
    window.show();
    return app.exec();
}
