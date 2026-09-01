#ifndef KARAOKEDSPDASHBOARD_H
#define KARAOKEDSPDASHBOARD_H

#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QProgressBar>
#include <QPushButton>
#include <QWidget>
#include <QVector>
#include <QTimer>
#include <QMediaPlayer>
#include <QProcess>
#include <vector>

#include "KeyDetector.h"
#include "TargetPitchTimeline.h"
#include "TargetMelodyLoader.h"
#include "PitchComparisonEngine.h"
#include "PerformanceAnalyzer.h"

class AudioDeviceManager;
class AudioStream;
class QAudioOutput;
class QVideoWidget;

class DashboardPitchGraph : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPitchGraph(QWidget* parent = nullptr);
    void addPoint(float inFreq, float tarFreq);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<float> inHistory;
    QVector<float> tarHistory;
    float mMinFreq{ 50.0f };
    float mMaxFreq{ 800.0f };
};

class KaraokeDSPDashboard : public QMainWindow {
    Q_OBJECT

public:
    explicit KaraokeDSPDashboard(QWidget* parent = nullptr);
    ~KaraokeDSPDashboard();

public slots:
    void updateReport(float inF, float tarF, float ratio, QString inN, QString tarN);

private slots:
    void onStartAudio();
    void onStopAudio();
    void onMicDeviceChanged(int index);
    void onEarphoneDeviceChanged(int index);
    void onVideoPositionChanged(qint64 position);
    void onVideoDurationChanged(qint64 duration);
    void onManualKeyChanged();
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void runConsoleEvaluation();
    void onVocalIsolationFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onVocalIsolationErrorOccurred(QProcess::ProcessError error);

private:
    void PopulateAudioDevices();
    void loadTargetMelodyForCurrentVideo(const QString& videoPath);
    void printConsoleFinalReport();
    void isolateVocalsFromSong(const QString& songPath);
    void analyzeVocalsFile(const QString& vocalsPath);
    void onComparisonReportReady(bool success, const QString& jsonPath, const QString& message = QString());
    void showVoiceComparisonReport();
    void restartSongDecoderForMixer();

    QAudioDecoder* m_songDecoder = nullptr;
    QAudioDecoder* m_vocalsDecoder = nullptr;
    void setupAudioDecoder();
    void setupVocalsDecoder();
    void onDecoderBufferReady();
    void onVocalsDecoderBufferReady();

    AudioDeviceManager* mDeviceManager{ nullptr };
    AudioStream* mAudioStream{ nullptr };
    QMediaPlayer* m_player{ nullptr };

    void setupSongDecoder(const QString& filePath);
    void onAudioBufferDecoded();
    QAudioOutput* m_earphoneAudio{ nullptr };
    QVideoWidget* videoWidget{ nullptr };

    KeyDetector mKeyDetector;

    QLabel* videoPathLabel{ nullptr };
    QLabel* timeLabel{ nullptr };
    QLabel* detectedKeyLabel{ nullptr };
    QLabel* inputFreqLabel{ nullptr };
    QLabel* correctedFreqLabel{ nullptr };
    QLabel* ratioLabel{ nullptr };
    QLabel* inputNoteLabel{ nullptr };
    QLabel* targetNoteLabel{ nullptr };
    QLabel* micVolLabel{ nullptr };
    QPushButton* recordMicBtn{ nullptr };
    QPushButton* viewComparisonBtn{ nullptr };
    QPushButton* openComparisonJsonBtn{ nullptr };
    QPushButton* openReportFolderBtn{ nullptr };
    QLabel* comparisonStatusLabel{ nullptr };
    QLabel* earphoneVolLabel{ nullptr };
    QLabel* pitchLabel{ nullptr };

    QComboBox* micComboBox{ nullptr };
    QComboBox* earphoneComboBox{ nullptr };
    QComboBox* keySelectComboBox{ nullptr };
    QComboBox* scaleTypeComboBox{ nullptr };

    QSlider* videoProgressSlider{ nullptr };
    QSlider* micVolSlider{ nullptr };
    QSlider* earphoneVolSlider{ nullptr };
    QSlider* pitchSlider{ nullptr };

    QPushButton* startAudioBtn{ nullptr };
    QPushButton* stopAudioBtn{ nullptr };
    QPushButton* refreshDevicesBtn{ nullptr };

    DashboardPitchGraph* pitchGraph{ nullptr };

    bool mAudioRunning{ false };
    bool m_videoSeeking{ false };

    float mLiveInputHz{ 0.0f };

    TargetPitchTimeline mTargetTimeline;
    PitchComparisonEngine mComparisonEngine;
    PerformanceAnalyzer mAnalyzer;
    QTimer mEvalTimer;
    bool mReportAlreadyPrinted{ false };
    QString mLoadedSongPath;
    QString mTargetJsonPath;
    QString mLastComparisonReportPath;
    bool mTargetLoadedFromExistingCache{ false };

    std::vector<float> mFullSongSamplesForAnalysis;
    int mSongSampleRateForAnalysis{ 44100 };

    std::vector<float> mVocalsSamplesForAnalysis;
    int mVocalsSampleRateForAnalysis{ 44100 };

    QProcess* m_vocalIsolationProcess{ nullptr };
    QString mVocalsFilePath;
    bool mVocalIsolationInProgress{ false };
};

#endif // KARAOKEDSPDASHBOARD_H