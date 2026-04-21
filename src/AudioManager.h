#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSoundEffect>
#include <QUrl>

enum class SoundCue {
    Start,
    Confirm,
    Collect,
    CollectSpecial,
    GameOver,
};

inline size_t qHash(SoundCue cue, size_t seed = 0) noexcept
{
    return ::qHash(static_cast<int>(cue), seed);
}

class AudioManager : public QObject {
    Q_OBJECT
public:
    explicit AudioManager(QObject *parent = nullptr);

    void play(SoundCue cue);
    void startAmbient();

private:
    QUrl createTone(const QString &name, float frequency, int durationMs, float volume);
    QUrl createAmbient();
    static QByteArray makeWav(float frequency, int durationMs, float volume);
    static QByteArray makeAmbientWav(int durationMs, float volume);

    QHash<SoundCue, QList<QSoundEffect *>> m_effects;
    QHash<SoundCue, int> m_nextVoice;
    QSoundEffect *m_ambient = nullptr;
};
