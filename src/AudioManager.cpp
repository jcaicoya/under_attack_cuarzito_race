#include "AudioManager.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtEndian>
#include <cmath>
#include <tuple>

AudioManager::AudioManager(QObject *parent) : QObject(parent)
{
    const QList<std::tuple<SoundCue, QString, float, int, float>> cues = {
        {SoundCue::Start,          "start",          620.f, 120, 0.24f},
        {SoundCue::Confirm,        "confirm",        480.f,  80, 0.20f},
        {SoundCue::Collect,        "collect",        880.f,  90, 0.20f},
        {SoundCue::CollectSpecial, "collect_special",1240.f, 130, 0.24f},
        {SoundCue::GameOver,       "game_over",      150.f, 260, 0.28f},
    };

    for (const auto &[cue, name, freq, duration, volume] : cues) {
        const QUrl url = createTone(name, freq, duration, volume);
        QList<QSoundEffect *> voices;
        for (int i = 0; i < 3; ++i) {
            auto *effect = new QSoundEffect(this);
            effect->setSource(url);
            effect->setLoopCount(1);
            effect->setVolume(0.75f);
            voices.append(effect);
        }
        m_effects.insert(cue, voices);
        m_nextVoice.insert(cue, 0);
    }

    m_ambient = new QSoundEffect(this);
    m_ambient->setSource(createAmbient());
    m_ambient->setLoopCount(QSoundEffect::Infinite);
    m_ambient->setVolume(0.16f);
}

void AudioManager::play(SoundCue cue)
{
    QList<QSoundEffect *> voices = m_effects.value(cue);
    if (voices.isEmpty())
        return;

    int index = m_nextVoice.value(cue) % voices.size();
    QSoundEffect *effect = voices[index];
    effect->stop();
    effect->play();
    m_nextVoice[cue] = (index + 1) % voices.size();
}

void AudioManager::startAmbient()
{
    if (m_ambient && !m_ambient->isPlaying())
        m_ambient->play();
}

QUrl AudioManager::createTone(const QString &name, float frequency, int durationMs, float volume)
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/cuarzito-preshow-audio";
    QDir().mkpath(basePath);
    const QString path = basePath + "/" + name + ".wav";

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(makeWav(frequency, durationMs, volume));

    return QUrl::fromLocalFile(path);
}

QUrl AudioManager::createAmbient()
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/cuarzito-preshow-audio";
    QDir().mkpath(basePath);
    const QString path = basePath + "/ambient.wav";

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(makeAmbientWav(2000, 0.18f));

    return QUrl::fromLocalFile(path);
}

QByteArray AudioManager::makeWav(float frequency, int durationMs, float volume)
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr int sampleRate = 44100;
    constexpr int channels = 1;
    constexpr int bitsPerSample = 16;
    const int sampleCount = sampleRate * durationMs / 1000;
    const int dataSize = sampleCount * channels * bitsPerSample / 8;

    QByteArray wav;
    wav.reserve(44 + dataSize);
    auto appendBytes = [&](const char *data, int size) {
        wav.append(data, size);
    };
    auto appendU16 = [&](quint16 value) {
        value = qToLittleEndian(value);
        appendBytes(reinterpret_cast<const char *>(&value), sizeof(value));
    };
    auto appendU32 = [&](quint32 value) {
        value = qToLittleEndian(value);
        appendBytes(reinterpret_cast<const char *>(&value), sizeof(value));
    };

    appendBytes("RIFF", 4);
    appendU32(36 + dataSize);
    appendBytes("WAVE", 4);
    appendBytes("fmt ", 4);
    appendU32(16);
    appendU16(1);
    appendU16(channels);
    appendU32(sampleRate);
    appendU32(sampleRate * channels * bitsPerSample / 8);
    appendU16(channels * bitsPerSample / 8);
    appendU16(bitsPerSample);
    appendBytes("data", 4);
    appendU32(dataSize);

    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float env = std::sin(PI * qMin(1.f, static_cast<float>(i) / sampleCount));
        const float sample = std::sin(2.f * PI * frequency * t) * volume * env;
        qint16 pcm = qToLittleEndian(static_cast<qint16>(sample * 32767.f));
        appendBytes(reinterpret_cast<const char *>(&pcm), sizeof(pcm));
    }

    return wav;
}

QByteArray AudioManager::makeAmbientWav(int durationMs, float volume)
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr int sampleRate = 44100;
    constexpr int channels = 1;
    constexpr int bitsPerSample = 16;
    const int sampleCount = sampleRate * durationMs / 1000;
    const int dataSize = sampleCount * channels * bitsPerSample / 8;

    QByteArray wav;
    wav.reserve(44 + dataSize);
    auto appendBytes = [&](const char *data, int size) {
        wav.append(data, size);
    };
    auto appendU16 = [&](quint16 value) {
        value = qToLittleEndian(value);
        appendBytes(reinterpret_cast<const char *>(&value), sizeof(value));
    };
    auto appendU32 = [&](quint32 value) {
        value = qToLittleEndian(value);
        appendBytes(reinterpret_cast<const char *>(&value), sizeof(value));
    };

    appendBytes("RIFF", 4);
    appendU32(36 + dataSize);
    appendBytes("WAVE", 4);
    appendBytes("fmt ", 4);
    appendU32(16);
    appendU16(1);
    appendU16(channels);
    appendU32(sampleRate);
    appendU32(sampleRate * channels * bitsPerSample / 8);
    appendU16(channels * bitsPerSample / 8);
    appendU16(bitsPerSample);
    appendBytes("data", 4);
    appendU32(dataSize);

    for (int i = 0; i < sampleCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float drone = std::sin(2.f * PI * 55.f * t) * 0.50f
                          + std::sin(2.f * PI * 82.5f * t) * 0.28f
                          + std::sin(2.f * PI * 110.f * t) * 0.18f;
        const float shimmer = std::sin(2.f * PI * 440.f * t) * 0.035f
                            + std::sin(2.f * PI * 660.f * t) * 0.020f;
        const float sample = qBound(-1.f, (drone + shimmer) * volume, 1.f);
        qint16 pcm = qToLittleEndian(static_cast<qint16>(sample * 32767.f));
        appendBytes(reinterpret_cast<const char *>(&pcm), sizeof(pcm));
    }

    return wav;
}
