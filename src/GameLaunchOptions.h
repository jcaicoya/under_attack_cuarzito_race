#pragma once

#include <QString>

struct GameLaunchOptions {
    bool testMode = false;
    bool traceMode = false;
    QString testName;
    QString trackResource = QStringLiteral(":/tracks/first_tunnel.json");
};
