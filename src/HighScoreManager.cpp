#include "HighScoreManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

HighScoreManager::HighScoreManager()
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(basePath);
    m_path = basePath + "/highscores.json";
    load();
}

bool HighScoreManager::qualifies(int score) const
{
    if (score <= 0)
        return false;
    return m_entries.size() < 10 || score > m_entries.last().score;
}

void HighScoreManager::addScore(const QString &name, int score)
{
    if (score <= 0)
        return;

    Entry entry;
    entry.name = name.left(3).toUpper();
    if (entry.name.trimmed().isEmpty())
        entry.name = "QTZ";
    entry.score = score;

    m_entries.append(entry);
    sortAndTrim();
    save();
}


void HighScoreManager::load()
{
    m_entries.clear();

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        seedDefaults();
        save();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        Entry entry;
        entry.name = obj.value("name").toString("QTZ").left(3).toUpper();
        entry.score = obj.value("score").toInt();
        if (entry.score > 0)
            m_entries.append(entry);
    }

    if (m_entries.isEmpty())
        seedDefaults();

    sortAndTrim();
}

void HighScoreManager::save() const
{
    QJsonArray array;
    for (const Entry &entry : m_entries) {
        QJsonObject obj;
        obj.insert("name", entry.name);
        obj.insert("score", entry.score);
        array.append(obj);
    }

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void HighScoreManager::seedDefaults()
{
    m_entries = {
        {"QTZ", 120},
        {"LUA", 95},
        {"MAX", 70},
        {"CUA", 45},
        {"POL", 30},
    };
}

void HighScoreManager::sortAndTrim()
{
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) {
        return a.score > b.score;
    });

    while (m_entries.size() > 10)
        m_entries.removeLast();
}
