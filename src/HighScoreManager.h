#pragma once

#include <QList>
#include <QString>

class HighScoreManager {
public:
    struct Entry {
        QString name;
        int score = 0;
    };

    HighScoreManager();

    const QList<Entry> &entries() const { return m_entries; }
    bool qualifies(int score) const;
    void addScore(const QString &name, int score);

private:
    void load();
    void save() const;
    void seedDefaults();
    void sortAndTrim();

    QList<Entry> m_entries;
    QString m_path;
};
