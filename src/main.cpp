#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <csignal>
#include "GameLaunchOptions.h"
#include "MainWindow.h"

static void handleSignal(int) { QApplication::quit(); }

int main(int argc, char *argv[])
{
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Cuarzito pre-show game");
    parser.addHelpOption();

    QCommandLineOption testOption(QStringList() << "test",
                                  "Run a named automated test track.",
                                  "name");
    parser.addOption(testOption);
    parser.process(a);

    GameLaunchOptions options;
    if (parser.isSet(testOption)) {
        const QString testName = parser.value(testOption).trimmed().toLower();
        if (testName == "downhill")
            options.trackResource = QStringLiteral(":/tracks/tests/downhill.json");
        else if (testName == "uphill")
            options.trackResource = QStringLiteral(":/tracks/tests/uphill.json");
        else if (testName == "vertical")
            options.trackResource = QStringLiteral(":/tracks/tests/vertical.json");

        if (!testName.isEmpty()) {
            options.testMode = true;
            options.traceMode = true;
            options.testName = testName;
        }
    }

    MainWindow w(options);
    w.showFullScreen();
    return a.exec();
}
