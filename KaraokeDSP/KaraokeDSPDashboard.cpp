#include "KaraokeDSPDashboard.h"
#include "SongAnalyzer.h"
#include <iostream>

#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QMediaDevices>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QtMath>
#include <QUrl>
#include <QTime>
#include <QAudioDevice>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDesktopServices>
#include <QTextEdit>
#include "AudioDeviceManager.h"
#include "AudioStream.h"
#include "KeyDetector.h"
#include "SongAnalyzer.h"

// ============================================================================
// DASHBOARD PITCH GRAPH IMPLEMENTATION
// ============================================================================
DashboardPitchGraph::DashboardPitchGraph(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(150);
    setStyleSheet("background: #0d1117; border-radius: 6px; border: 1px solid #1f2937;");
}

void DashboardPitchGraph::addPoint(float inFreq, float tarFreq) {
    inHistory.append(inFreq);
    tarHistory.append(tarFreq);

    if (inHistory.size() > 200) inHistory.removeFirst();
    if (tarHistory.size() > 200) tarHistory.removeFirst();

    update();
}

void DashboardPitchGraph::clear() {
    inHistory.clear();
    tarHistory.clear();
    update();
}

void DashboardPitchGraph::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    painter.fillRect(rect(), QColor("#0d1117"));

    if (inHistory.isEmpty()) return;

    float xStep = static_cast<float>(w) / 200.0f;

    // Draw Target Pitch Line (Green)
    QPen greenPen(QColor("#22c55e"), 2);
    painter.setPen(greenPen);
    for (int i = 0; i < tarHistory.size() - 1; ++i) {
        if (tarHistory[i] <= 0 || tarHistory[i + 1] <= 0) continue;
        float x1 = i * xStep;
        float y1 = h - ((tarHistory[i] - mMinFreq) / (mMaxFreq - mMinFreq)) * h;
        float x2 = (i + 1) * xStep;
        float y2 = h - ((tarHistory[i + 1] - mMinFreq) / (mMaxFreq - mMinFreq)) * h;
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // Draw User Input Pitch Line (Red)
    QPen redPen(QColor("#ef4444"), 2);
    painter.setPen(redPen);
    for (int i = 0; i < inHistory.size() - 1; ++i) {
        if (inHistory[i] <= 0 || inHistory[i + 1] <= 0) continue;
        float x1 = i * xStep;
        float y1 = h - ((inHistory[i] - mMinFreq) / (mMaxFreq - mMinFreq)) * h;
        float x2 = (i + 1) * xStep;
        float y2 = h - ((inHistory[i + 1] - mMinFreq) / (mMaxFreq - mMinFreq)) * h;
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
}

KaraokeDSPDashboard::KaraokeDSPDashboard(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Karaoke DSP Engine - Studio Processing with Key Selection");
    resize(1350, 900);
    setStyleSheet(
        "QMainWindow{background:#0a0d14;}"
        "QWidget{color:#cbd5e1; font-family:'Segoe UI'; font-size:13px;}"
        "QGroupBox{border:1px solid #1f2937; border-radius:8px; margin-top:14px; padding-top:18px; background:#12151f;}"
        "QGroupBox::title{color:#3a86ff; subcontrol-origin:margin; left:12px;}"
        "QComboBox,QPushButton{background:#1a1e2b; border:1px solid #2a3042; padding:6px 8px; border-radius:6px;}"
        "QComboBox:hover,QPushButton:hover{border-color:#3a86ff;}"
        "QComboBox::drop-down{border:none; width:24px;}"
        "QComboBox QAbstractItemView{background:#1a1e2b; color:#cbd5e1; border:1px solid #2a3042; outline:none; selection-background-color:#3a86ff; selection-color:#ffffff; padding:4px;}"
        "QSlider::groove:horizontal{height:4px; background:#1f2333; border-radius:2px;}"
        "QSlider::handle:horizontal{background:#3a86ff; width:14px; height:14px; border-radius:7px; margin:-5px 0;}"
    );

    mDeviceManager = new AudioDeviceManager();
    mAudioStream = new AudioStream(this);
    m_vocalIsolationProcess = new QProcess(this);
    connect(m_vocalIsolationProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &KaraokeDSPDashboard::onVocalIsolationFinished);
    connect(m_vocalIsolationProcess, &QProcess::errorOccurred,
        this, &KaraokeDSPDashboard::onVocalIsolationErrorOccurred);
    setupAudioDecoder();
    setupVocalsDecoder();
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLay = new QVBoxLayout(central);
    mainLay->setSpacing(8);
    mainLay->setContentsMargins(10, 10, 10, 10);

    QHBoxLayout* tabLay = new QHBoxLayout();
    QLabel* tab1 = new QLabel("REAL-TIME MEDIA ENGINE - LIVE DASHBOARD");
    tab1->setStyleSheet("color:#3a86ff; font-weight:bold; border-bottom:2px solid #3a86ff; padding-bottom:4px;");
    QLabel* tab2 = new QLabel("MANUAL KEY SCALE LOCK ACTIVE");
    tab2->setStyleSheet("color:#a855f7; font-size:11px; background:#241836; padding:4px 8px; border-radius:10px; font-weight:bold; border:1px solid #a855f7;");
    tabLay->addWidget(tab1);
    tabLay->addWidget(tab2);
    tabLay->addStretch();
    mainLay->addLayout(tabLay);

    videoWidget = new QVideoWidget();
    videoWidget->setMinimumHeight(180);
    videoWidget->setMaximumHeight(200);
    videoWidget->setStyleSheet("background:#000; border-radius:8px; border:1px solid #1f2937;");

    m_player = new QMediaPlayer(this);
    m_player->setVideoOutput(videoWidget);
    m_player->setAudioOutput(nullptr);

    mainLay->addWidget(videoWidget);

    QHBoxLayout* pathLay = new QHBoxLayout();
    videoPathLabel = new QLabel("Select Karaoke Video...", this);
    videoPathLabel->setStyleSheet("background:#1a1e2b; padding:6px; border-radius:6px; color:#64748b; font-size:11px;");
    QPushButton* browseBtn = new QPushButton("Browse", this);
    browseBtn->setFixedWidth(80);
    QPushButton* playBtn = new QPushButton("PLAY", this);
    playBtn->setStyleSheet("background:#3a86ff; color:white; font-weight:bold; min-width:80px; padding:6px;");
    QPushButton* stopBtn = new QPushButton("STOP", this);

    // Fix shadowing warning by using local distinct name before member assignment
    QPushButton* recordMicButton = new QPushButton("Record Mic", this);
    recordMicButton->setCheckable(true);
    recordMicButton->setStyleSheet("background:#f59e0b; color:#000; font-weight:bold; padding:6px; border-radius:6px;");
    this->recordMicBtn = recordMicButton;

    stopBtn->setFixedWidth(70);
    pathLay->addWidget(videoPathLabel, 1);
    pathLay->addWidget(browseBtn);
    pathLay->addWidget(playBtn);
    pathLay->addWidget(recordMicButton);
    pathLay->addWidget(stopBtn);
    mainLay->addLayout(pathLay);

    QHBoxLayout* videoSeekLay = new QHBoxLayout();
    timeLabel = new QLabel("00:00 / 00:00", this);
    timeLabel->setStyleSheet("color:#3a86ff; font-size:11px; background:transparent; border:none; min-width:90px;");
    videoProgressSlider = new QSlider(Qt::Horizontal, this);
    videoProgressSlider->setRange(0, 1000);
    videoProgressSlider->setValue(0);
    videoProgressSlider->setStyleSheet("QSlider::groove:horizontal{height:6px; background:#1f2333; border-radius:3px;} QSlider::handle:horizontal{background:#3a86ff; width:12px; height:12px; border-radius:6px; margin:-3px 0;} QSlider::sub-page:horizontal{background:#3a86ff; border-radius:3px;}");
    videoSeekLay->addWidget(timeLabel);
    videoSeekLay->addWidget(videoProgressSlider, 1);
    mainLay->addLayout(videoSeekLay);

    QHBoxLayout* bottomLay = new QHBoxLayout();
    bottomLay->setSpacing(10);

    QGroupBox* routingGroup = new QGroupBox("HARDWARE ROUTING & KEY CONTROL", this);
    QVBoxLayout* routingLay = new QVBoxLayout(routingGroup);
    routingLay->setSpacing(10);

    QHBoxLayout* refreshLay = new QHBoxLayout();
    refreshDevicesBtn = new QPushButton("Refresh Devices", this);
    refreshDevicesBtn->setStyleSheet("background:#1a1e2b; color:#3a86ff; padding:5px 10px; border-radius:6px; border:1px solid #3a86ff; font-size:11px;");
    refreshLay->addStretch();
    refreshLay->addWidget(refreshDevicesBtn);
    routingLay->addLayout(refreshLay);

    QHBoxLayout* keyDisplayLay = new QHBoxLayout();
    QLabel* keyTitleLabel = new QLabel("Song Musical Scale:", this);
    keyTitleLabel->setStyleSheet("color:#a855f7; font-size:11px; font-weight:bold; background:transparent; border:none;");

    keySelectComboBox = new QComboBox(this);
    keySelectComboBox->addItems({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" });
    keySelectComboBox->setCurrentIndex(2);

    scaleTypeComboBox = new QComboBox(this);
    scaleTypeComboBox->addItems({ "Major", "Minor", "Chromatic" });
    scaleTypeComboBox->setCurrentIndex(0);

    keyDisplayLay->addWidget(keyTitleLabel);
    keyDisplayLay->addWidget(keySelectComboBox);
    keyDisplayLay->addWidget(scaleTypeComboBox);
    routingLay->addLayout(keyDisplayLay);

    auto addDeviceWithVolume = [&](QString title, QComboBox*& combo, QSlider*& volSlider, QLabel*& volLabel, QString volColor) {
        QLabel* l = new QLabel(title, this);
        l->setStyleSheet("color:#94a3b8; font-size:11px; background:transparent; border:none; font-weight:bold;");
        combo = new QComboBox(this);
        combo->setMinimumHeight(32);
        routingLay->addWidget(l);
        routingLay->addWidget(combo);

        QHBoxLayout* volLay = new QHBoxLayout();
        QLabel* volTitle = new QLabel("Volume:", this);
        volTitle->setStyleSheet("background:transparent; border:none; font-size:10px; color:#64748b; min-width:50px;");
        volSlider = new QSlider(Qt::Horizontal, this);
        volSlider->setRange(0, 100);
        volSlider->setValue(80);
        volSlider->setStyleSheet(QString("QSlider::groove:horizontal{height:4px; background:#1f2333;} QSlider::handle:horizontal{background:%1; width:12px; height:12px; border-radius:6px; margin:-4px 0;}").arg(volColor));
        volLabel = new QLabel("80%", this);
        volLabel->setFixedWidth(35);
        volLabel->setStyleSheet(QString("color:%1; background:transparent; border:none; font-size:11px; font-weight:bold;").arg(volColor));
        volLay->addWidget(volTitle);
        volLay->addWidget(volSlider, 1);
        volLay->addWidget(volLabel);
        routingLay->addLayout(volLay);
        };

    addDeviceWithVolume("Mic Input:", micComboBox, micVolSlider, micVolLabel, "#ef4444");
    addDeviceWithVolume("Earphone Monitor (Song):", earphoneComboBox, earphoneVolSlider, earphoneVolLabel, "#3a86ff");

    auto addSliderRow = [&](QString title, QSlider*& sl, QLabel*& valLab, int initVal, QString unit, QString color) {
        QHBoxLayout* r = new QHBoxLayout();
        QLabel* t = new QLabel(title, this);
        t->setFixedWidth(95);
        t->setStyleSheet("background:transparent; border:none; font-size:11px;");
        sl = new QSlider(Qt::Horizontal, this);
        sl->setRange(0, 100);
        sl->setValue(initVal);
        valLab = new QLabel(QString("%1%2").arg(initVal - 50).arg(unit), this);
        valLab->setFixedWidth(90);
        valLab->setStyleSheet(QString("color:%1; background:transparent; border:none; font-size:11px; font-weight:bold;").arg(color));
        r->addWidget(t);
        r->addWidget(sl, 1);
        r->addWidget(valLab);
        routingLay->addLayout(r);
        };

    addSliderRow("Pitch Shift:", pitchSlider, pitchLabel, 50, " Semi", "#a855f7");

    QHBoxLayout* btnLay = new QHBoxLayout();
    startAudioBtn = new QPushButton("Start Audio", this);
    startAudioBtn->setStyleSheet("background:#1a2e1a; color:#22c55e; font-weight:bold; padding:8px; border-radius:6px; border:1px solid #22c55e;");
    stopAudioBtn = new QPushButton("Stop", this);
    stopAudioBtn->setStyleSheet("background:#2e1a1a; color:#ef4444; padding:8px; border-radius:6px; border:1px solid #ef4444;");
    btnLay->addWidget(startAudioBtn, 2);
    btnLay->addWidget(stopAudioBtn, 1);
    routingLay->addLayout(btnLay);

    QLabel* hint = new QLabel("SCALE LOCK ACTIVE - Choose target key above for accurate tuning", this);
    hint->setStyleSheet("background:#1e293b; padding:6px; border-radius:4px; color:#a855f7; font-size:10px; border:1px solid #a855f7; font-weight:bold;");
    routingLay->addWidget(hint);
    routingLay->addStretch();

    QGroupBox* reportGroup = new QGroupBox("MASTER OUTPUT - LIVE PROCESSED MONITOR", this);
    QVBoxLayout* reportLay = new QVBoxLayout(reportGroup);
    reportLay->setSpacing(8);

    QHBoxLayout* freqRow = new QHBoxLayout();
    inputFreqLabel = new QLabel("Input: -- Hz", this);
    inputFreqLabel->setStyleSheet("color:#ef4444; font-size:14px; font-weight:bold; background:#1a1212; padding:8px; border-radius:6px; border:1px solid #ef4444;");
    correctedFreqLabel = new QLabel("Corrected: -- Hz", this);
    correctedFreqLabel->setStyleSheet("color:#22c55e; font-size:14px; font-weight:bold; background:#121a12; padding:8px; border-radius:6px; border:1px solid #22c55e;");
    freqRow->addWidget(inputFreqLabel);
    freqRow->addWidget(correctedFreqLabel);
    reportLay->addLayout(freqRow);

    ratioLabel = new QLabel("Correction Ratio: 1.000x | Waiting for mic signal", this);
    ratioLabel->setStyleSheet("background:#1e293b; padding:6px; border-radius:20px; color:#3a86ff; border:none; font-size:11px;");
    reportLay->addWidget(ratioLabel);

    QHBoxLayout* noteRow = new QHBoxLayout();
    inputNoteLabel = new QLabel("Detected: ---", this);
    inputNoteLabel->setStyleSheet("background:#1a1212; padding:6px; border-radius:6px; color:#ef4444; border:1px solid #331a1a;");
    targetNoteLabel = new QLabel("Target: --- (Pure Processed)", this);
    targetNoteLabel->setStyleSheet("background:#121a12; padding:6px; border-radius:6px; color:#22c55e; border:1px solid #1a331a;");
    noteRow->addWidget(inputNoteLabel);
    noteRow->addWidget(targetNoteLabel);
    reportLay->addLayout(noteRow);

    pitchGraph = new DashboardPitchGraph(this);
    reportLay->addWidget(pitchGraph, 1);

    QHBoxLayout* legendLay = new QHBoxLayout();
    QLabel* redDot = new QLabel("Input (User Voice)", this);
    redDot->setStyleSheet("color:#ef4444; background:transparent; border:none; font-size:11px; font-weight:bold;");
    QLabel* greenDot = new QLabel("Corrected (Pure Processed -> Speaker)", this);
    greenDot->setStyleSheet("color:#22c55e; background:transparent; border:none; font-size:11px; font-weight:bold;");
    legendLay->addWidget(redDot);
    legendLay->addWidget(greenDot);
    legendLay->addStretch();
    reportLay->addLayout(legendLay);

    QHBoxLayout* comparisonBtnLay = new QHBoxLayout();
    viewComparisonBtn = new QPushButton("View Voice Comparison Report", this);
    viewComparisonBtn->setEnabled(false);
    viewComparisonBtn->setStyleSheet("background:#1a1e2b; color:#94a3b8; font-weight:bold; padding:8px; border-radius:6px; border:1px solid #2a3042;");
    openComparisonJsonBtn = new QPushButton("Open JSON", this);
    openComparisonJsonBtn->setEnabled(false);
    openReportFolderBtn = new QPushButton("Open Report Folder", this);
    comparisonBtnLay->addWidget(viewComparisonBtn, 2);
    comparisonBtnLay->addWidget(openComparisonJsonBtn, 1);
    comparisonBtnLay->addWidget(openReportFolderBtn, 1);
    reportLay->addLayout(comparisonBtnLay);
    comparisonStatusLabel = new QLabel("Comparison report: none yet", this);
    comparisonStatusLabel->setStyleSheet("color:#64748b; font-size:11px; background:transparent; border:none;");
    reportLay->addWidget(comparisonStatusLabel);

    bottomLay->addWidget(routingGroup, 2);
    bottomLay->addWidget(reportGroup, 3);
    mainLay->addLayout(bottomLay, 1);

    // CONNECTIONS SECTION
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, "Select Video", "", "Video (*.mp4 *.mkv *.avi *.mp3 *.wav)");
        if (!f.isEmpty()) {
            m_player->setSource(QUrl::fromLocalFile(f));

            mFullSongSamplesForAnalysis.clear();
            mLoadedSongPath = f;
            videoPathLabel->setText(f);
            mKeyDetector.Reset();

            setupSongDecoder(f);
            if (mAudioStream) mAudioStream->SetSongPath(f.toStdString());

            loadTargetMelodyForCurrentVideo(f);
            if (!mTargetLoadedFromExistingCache)
                isolateVocalsFromSong(f);
        }
        });

    connect(playBtn, &QPushButton::clicked, this, [this]() {
        mAnalyzer.Reset();
        mReportAlreadyPrinted = false;
        m_player->play();
        mEvalTimer.start(100);
        });

    connect(this->recordMicBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (!mAudioStream) return;
        if (!mAudioRunning) {
            std::cout << "[Dashboard] Cannot start mic capture: audio not running. Start audio first." << std::endl;
            if (this->recordMicBtn) this->recordMicBtn->setChecked(false);
            return;
        }
        if (checked) {
            try {
                mAudioStream->StartMicCapture();
                std::cout << "[Dashboard] Mic capture enabled" << std::endl;
                if (this->recordMicBtn) this->recordMicBtn->setText("Recording...");
            }
            catch (const std::exception& e) {
                std::cerr << "[Dashboard] Exception starting mic capture: " << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "[Dashboard] Unknown exception starting mic capture" << std::endl;
            }
        }
        else {
            try {
                QString user = qgetenv("USERNAME");
                if (user.isEmpty()) user = "localUser";
                QFileInfo fi(mLoadedSongPath);
                QString base = fi.completeBaseName();
                mAudioStream->StopMicCaptureAndAnalyze(user.toStdString(), base.toStdString());
                std::cout << "[Dashboard] Mic capture disabled, analysis started" << std::endl;
                if (this->recordMicBtn) this->recordMicBtn->setText("Record Mic");
            }
            catch (const std::exception& e) {
                std::cerr << "[Dashboard] Exception stopping mic capture: " << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "[Dashboard] Unknown exception stopping mic capture" << std::endl;
            }
        }
        });

    if (mAudioStream) {
        mAudioStream->SetComparisonReportCallback([this](bool ok, const std::string& path, const std::string& message) {
            const QString qpath = QString::fromStdString(path);
            const QString qmsg = QString::fromStdString(message);
            QMetaObject::invokeMethod(this, [this, ok, qpath, qmsg]() {
                onComparisonReportReady(ok, qpath, qmsg);
                }, Qt::QueuedConnection);
            });
    }
    connect(viewComparisonBtn, &QPushButton::clicked, this, &KaraokeDSPDashboard::showVoiceComparisonReport);
    connect(openComparisonJsonBtn, &QPushButton::clicked, this, [this]() {
        if (mLastComparisonReportPath.isEmpty()) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(mLastComparisonReportPath));
        });
    connect(openReportFolderBtn, &QPushButton::clicked, this, [this]() {
        QString user = qgetenv("USERNAME");
        if (user.isEmpty()) user = "localUser";
        QString dir = "D:/SwaraagyaSoftware_08_08/cache/user_reports/" + user;
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });

    connect(stopBtn, &QPushButton::clicked, this, [this]() {
        mEvalTimer.stop();
        m_player->stop();
        printConsoleFinalReport();
        });

    connect(videoProgressSlider, &QSlider::sliderPressed, this, [this]() { m_videoSeeking = true; });
    connect(videoProgressSlider, &QSlider::sliderReleased, this, [this]() {
        m_videoSeeking = false;
        qint64 pos = m_player->duration() * static_cast<qint64>(videoProgressSlider->value()) / 1000;
        m_player->setPosition(pos);
        });

    connect(m_player, &QMediaPlayer::positionChanged, this, &KaraokeDSPDashboard::onVideoPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &KaraokeDSPDashboard::onVideoDurationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &KaraokeDSPDashboard::onMediaStatusChanged);

    connect(micComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KaraokeDSPDashboard::onMicDeviceChanged);
    connect(earphoneComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KaraokeDSPDashboard::onEarphoneDeviceChanged);

    connect(keySelectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KaraokeDSPDashboard::onManualKeyChanged);
    connect(scaleTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KaraokeDSPDashboard::onManualKeyChanged);

    connect(micVolSlider, &QSlider::valueChanged, this, [this](int v) {
        micVolLabel->setText(QString("%1%").arg(v));
        if (mAudioStream) {
            mAudioStream->SetMicVolume(static_cast<float>(v) / 100.0f);
        }
        });

    connect(earphoneVolSlider, &QSlider::valueChanged, this, [this](int v) {
        earphoneVolLabel->setText(QString("%1%").arg(v));
        if (mAudioStream) {
            mAudioStream->SetEarphoneVolume(static_cast<float>(v) / 100.0f);
        }
        });

    connect(pitchSlider, &QSlider::valueChanged, this, [this](int v) {
        int semitones = v - 50;
        pitchLabel->setText(QString("%1 Semi").arg(semitones));
        if (mAudioStream) mAudioStream->SetPitchShift(static_cast<float>(semitones));
        });

    connect(startAudioBtn, &QPushButton::clicked, this, &KaraokeDSPDashboard::onStartAudio);
    connect(stopAudioBtn, &QPushButton::clicked, this, &KaraokeDSPDashboard::onStopAudio);
    connect(refreshDevicesBtn, &QPushButton::clicked, this, &KaraokeDSPDashboard::PopulateAudioDevices);

    connect(&mEvalTimer, &QTimer::timeout, this, &KaraokeDSPDashboard::runConsoleEvaluation);

    PopulateAudioDevices();
    onManualKeyChanged();
}

KaraokeDSPDashboard::~KaraokeDSPDashboard() {
    if (mDeviceManager) {
        delete mDeviceManager;
        mDeviceManager = nullptr;
    }
    if (mAudioStream) {
        mAudioStream->Stop();
        mAudioStream->deleteLater();
    }
}

// ============================================================================
// IMPLEMENTATION OF MISSING REPORT SLOTS (FIXES UNRESOLVED EXTERNAL SYMBOLS)
// ============================================================================
void KaraokeDSPDashboard::onComparisonReportReady(bool success, const QString& jsonPath, const QString& message) {
    if (success) {
        mLastComparisonReportPath = jsonPath;
        if (viewComparisonBtn) viewComparisonBtn->setEnabled(true);
        if (openComparisonJsonBtn) openComparisonJsonBtn->setEnabled(true);
        if (comparisonStatusLabel) {
            comparisonStatusLabel->setText("Comparison report ready: " + QFileInfo(jsonPath).fileName());
        }
    }
    else {
        // P7: surface the exact reason so the user knows why no report was produced.
        if (viewComparisonBtn) viewComparisonBtn->setEnabled(false);
        if (openComparisonJsonBtn) openComparisonJsonBtn->setEnabled(false);
        const QString reason = message.isEmpty() ? QString("Failed to generate comparison report.") : message;
        if (comparisonStatusLabel) {
            comparisonStatusLabel->setText("No report: " + reason);
        }
        std::cerr << "[Dashboard] Comparison report failed: " << reason.toStdString() << std::endl;
    }
}

void KaraokeDSPDashboard::showVoiceComparisonReport() {
    if (mLastComparisonReportPath.isEmpty()) {
        QMessageBox::information(this, "Report Unavailable", "No voice comparison report has been generated yet.");
        return;
    }

    QFile file(mLastComparisonReportPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Unable to open comparison report JSON file.");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(this, "Error", "Invalid report format.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Voice Comparison Report");
    dialog.resize(650, 450);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QTextEdit* textEdit = new QTextEdit(&dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(doc.toJson(QJsonDocument::Indented));
    layout->addWidget(textEdit);

    dialog.exec();
}

void KaraokeDSPDashboard::loadTargetMelodyForCurrentVideo(const QString& videoPath) {
    mLoadedSongPath = videoPath;
    mTargetTimeline.Clear();
    mTargetLoadedFromExistingCache = false;
    mTargetJsonPath.clear();

    QFileInfo fileInfo(videoPath);
    QString jsonPath = fileInfo.absolutePath() + QDir::separator() + fileInfo.completeBaseName() + ".json";

    if (QFileInfo::exists(jsonPath)) {
        if (TargetMelodyLoader::LoadFromFile(jsonPath.toStdString(), mTargetTimeline)) {
            size_t voiced = 0;
            for (const auto& f : mTargetTimeline.GetFramesRef()) if (f.isVoiced && f.targetHz > 0.0f) ++voiced;
            std::cout << "[TargetDebug] TargetMelodyLoader loaded " << mTargetTimeline.GetFrameCount()
                << " frames, voiced=" << voiced << " from " << jsonPath.toStdString() << std::endl;
            if (voiced > 0) {
                std::cout << "[Target Melody] Loaded melody JSON successfully: " << jsonPath.toStdString() << std::endl;
                mTargetJsonPath = jsonPath;
                mTargetLoadedFromExistingCache = true;
                if (mAudioStream) {
                    mAudioStream->SetTargetPitchTimeline(&mTargetTimeline);
                    mAudioStream->SetTargetJsonPath(jsonPath.toStdString());
                    mAudioStream->EnableMelodyCorrection(true);
                    mAudioStream->SetCorrectionStrength(1.0f);
                }
                return;
            }
            std::cout << "[TargetDebug] Ignoring " << jsonPath.toStdString()
                << " (0 voiced frames); will try cache / regeneration." << std::endl;
            mTargetTimeline.Clear();
        }
        else {
            std::cout << "[Target Melody] Failed to parse JSON file: " << jsonPath.toStdString() << std::endl;
        }
    }

    std::string loadedPath;
    if (SongAnalyzer::TryLoadExistingCache(mTargetTimeline, videoPath.toStdString(), loadedPath)) {
        size_t voiced = 0;
        for (const auto& f : mTargetTimeline.GetFramesRef()) if (f.isVoiced && f.targetHz > 0.0f) ++voiced;
        if (voiced == 0) {
            std::cout << "[TargetDebug] TryLoadExistingCache returned a 0-voiced timeline; ignoring "
                << loadedPath << std::endl;
            mTargetTimeline.Clear();
        }
        else {
            mTargetJsonPath = QString::fromStdString(loadedPath);
            mTargetLoadedFromExistingCache = true;
            if (mAudioStream) {
                mAudioStream->SetTargetPitchTimeline(&mTargetTimeline);
                mAudioStream->SetTargetJsonPath(loadedPath);
                mAudioStream->EnableMelodyCorrection(true);
                mAudioStream->SetCorrectionStrength(1.0f);
            }
            std::cout << "[TargetDebug] Existing SongAnalyzer cache loaded with voiced=" << voiced
                << " frames: " << loadedPath << " (will not be regenerated)." << std::endl;
            return;
        }
    }

    std::cout << "[Target Melody] No existing target JSON found in cache. Isolation/analysis may run if needed." << std::endl;
}

void KaraokeDSPDashboard::runConsoleEvaluation() {
    if (m_player->playbackState() != QMediaPlayer::PlayingState) return;

    double currentTimeSec = static_cast<double>(m_player->position()) / 1000.0;
    float userHz = mAudioStream ? mAudioStream->GetCurrentPitchHz() : mLiveInputHz;
    mLiveInputHz = userHz;

    TargetPitchFrame frame = mTargetTimeline.GetTargetPitchAt(currentTimeSec);
    float targetHz = frame.targetHz > 0.0f ? frame.targetHz : frame.targetF0;

    PitchComparisonResult eval = mComparisonEngine.EvaluateDirect(currentTimeSec, userHz, 1.0f, targetHz);
    mAnalyzer.AddResult(eval);

    QString inN = QString::fromUtf8(eval.userNote);
    QString tarN = QString::fromUtf8(eval.targetNote);
    float ratio = (userHz > 0.0f && targetHz > 0.0f) ? (targetHz / userHz) : 1.0f;
    updateReport(userHz, targetHz, ratio, inN, tarN);
}

void KaraokeDSPDashboard::printConsoleFinalReport() {
    if (mReportAlreadyPrinted) return;
    mReportAlreadyPrinted = true;

    PerformanceReport rep = mAnalyzer.GetReport();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "========================================\n";
    std::cout << "        KARAOKE PITCH REPORT\n";
    std::cout << "========================================\n\n";
    std::cout << "Total Singing Time : " << rep.songDurationSec << " sec\n\n";
    std::cout << "IN TUNE            : " << rep.inTunePercent << " %\n";
    std::cout << "SLIGHTLY OFF       : " << rep.slightlyOffPercent << " %\n";
    std::cout << "OUT OF TUNE        : " << rep.outOfTunePercent << " %\n\n";
    std::cout << "Average Error      : " << rep.avgPitchErrorCents << " cents\n";
    std::cout << "FINAL SCORE        : " << rep.overallVocalAccuracy << " / 100\n\n";
    std::cout << "========================================\n" << std::endl;
}

void KaraokeDSPDashboard::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        mEvalTimer.stop();
        printConsoleFinalReport();
    }
}

void KaraokeDSPDashboard::PopulateAudioDevices() {
    micComboBox->blockSignals(true);
    earphoneComboBox->blockSignals(true);

    micComboBox->clear();
    earphoneComboBox->clear();

    QList<QAudioDevice> inputDevices = QMediaDevices::audioInputs();
    QList<QAudioDevice> outputDevices = QMediaDevices::audioOutputs();

    QAudioDevice defaultInput = QMediaDevices::defaultAudioInput();
    QAudioDevice defaultOutput = QMediaDevices::defaultAudioOutput();

    for (int i = 0; i < inputDevices.size(); ++i) {
        QString devName = inputDevices[i].description();
        QString label = devName + (inputDevices[i] == defaultInput ? " (Default)" : "");
        micComboBox->addItem(label, i);
    }

    for (int i = 0; i < outputDevices.size(); ++i) {
        QString devName = outputDevices[i].description();
        QString label = devName + (outputDevices[i] == defaultOutput ? " (Default)" : "");
        earphoneComboBox->addItem(label, i);
    }

    if (micComboBox->count() == 0) micComboBox->addItem("Default Microphone", -1);
    if (earphoneComboBox->count() == 0) earphoneComboBox->addItem("Default Speakers", -1);

    std::cout << "[Dashboard] Populated " << micComboBox->count() << " input devices" << std::endl;
    std::cout << "[Dashboard] Populated " << earphoneComboBox->count() << " output devices" << std::endl;

    micComboBox->blockSignals(false);
    earphoneComboBox->blockSignals(false);
}

void KaraokeDSPDashboard::onManualKeyChanged() {
    int rootNote = keySelectComboBox->currentIndex();
    int scaleIdx = scaleTypeComboBox->currentIndex();

    KeyScaleMatcher::ScaleType scaleType = KeyScaleMatcher::MAJOR;
    if (scaleIdx == 1) scaleType = KeyScaleMatcher::MINOR;
    else if (scaleIdx == 2) scaleType = KeyScaleMatcher::CHROMATIC;

    if (mAudioStream) {
        mAudioStream->SetMusicalKey(rootNote, scaleType);
    }
}

void KaraokeDSPDashboard::onStartAudio() {
    if (mAudioRunning) { mAudioStream->Stop(); }

    int micIdx = micComboBox->currentData().toInt();
    int spkIdx = earphoneComboBox->currentData().toInt();

    if (!mAudioStream->Open(micIdx, spkIdx)) {
        QMessageBox::warning(this, "Audio Error", "Mic/Speaker open failed");
        return;
    }

    if (m_songDecoder && !mLoadedSongPath.isEmpty()) {
        QAudioFormat songFormat;
        songFormat.setSampleRate(mAudioStream->GetSampleRate());
        songFormat.setChannelCount(mAudioStream->GetChannelCount());
        songFormat.setSampleFormat(QAudioFormat::Float);
        m_songDecoder->stop();
        m_songDecoder->setAudioFormat(songFormat);
        mAudioStream->ClearSongBuffer();
        m_songDecoder->setSource(mLoadedSongPath);
        m_songDecoder->start();
    }

    mAudioStream->SetMicVolume(static_cast<float>(micVolSlider->value()) / 100.0f);
    mAudioStream->SetSpeakerVolume(80.0f / 100.0f);
    mAudioStream->SetEarphoneVolume(static_cast<float>(earphoneVolSlider->value()) / 100.0f);
    onManualKeyChanged();

    if (!mAudioStream->Start()) {
        QMessageBox::warning(this, "Audio Error", "Audio start failed");
        return;
    }

    mAudioRunning = true;
    startAudioBtn->setText("Audio Running");
    startAudioBtn->setStyleSheet("background:#22c55e; color:#000; font-weight:bold; padding:8px; border-radius:6px;");
    pitchGraph->clear();
}

void KaraokeDSPDashboard::onStopAudio() {
    if (mAudioStream) mAudioStream->Stop();
    mEvalTimer.stop();
    m_player->stop();
    printConsoleFinalReport();
    mAudioRunning = false;
    startAudioBtn->setText("Start Audio");
    startAudioBtn->setStyleSheet("background:#1a2e1a; color:#22c55e; font-weight:bold; padding:8px; border-radius:6px; border:1px solid #22c55e;");
}

void KaraokeDSPDashboard::onMicDeviceChanged(int index) {
    Q_UNUSED(index);
    if (mAudioRunning) onStartAudio();
}

void KaraokeDSPDashboard::onEarphoneDeviceChanged(int index) {
    Q_UNUSED(index);
    if (mAudioRunning) onStartAudio();
}

void KaraokeDSPDashboard::onVideoPositionChanged(qint64 position) {
    if (!m_videoSeeking) {
        qint64 duration = m_player->duration();
        if (duration > 0) {
            videoProgressSlider->setValue(static_cast<int>(position * 1000 / duration));
        }

        int hours = static_cast<int>((position / 3600000) % 60);
        int mins = static_cast<int>((position / 60000) % 60);
        int secs = static_cast<int>((position / 1000) % 60);
        QTime currentTime(hours, mins, secs);

        int durMins = static_cast<int>((duration / 60000) % 60);
        int durSecs = static_cast<int>((duration / 1000) % 60);
        QTime totalTime(0, durMins, durSecs);

        timeLabel->setText(QString("%1 / %2").arg(currentTime.toString("mm:ss")).arg(totalTime.toString("mm:ss")));

        if (mAudioStream) {
            double positionSeconds = static_cast<double>(position) / 1000.0;
            mAudioStream->SetSongPlaybackPosition(positionSeconds);
        }
    }
}

void KaraokeDSPDashboard::onVideoDurationChanged(qint64 duration) {
    Q_UNUSED(duration);
    videoProgressSlider->setRange(0, 1000);
}

void KaraokeDSPDashboard::updateReport(float inF, float tarF, float ratio, QString inN, QString tarN) {
    mLiveInputHz = inF;

    if (inF > 0.0f) {
        inputFreqLabel->setText(QString("Input: %1 Hz (%2)").arg(inF, 0, 'f', 1).arg(inN));
        correctedFreqLabel->setText(QString("Corrected: %1 Hz (%2)").arg(tarF, 0, 'f', 1).arg(tarN));
        ratioLabel->setText(QString("Ratio: %1x | %2 -> %3 | Pure Processed Voice -> Speaker").arg(ratio, 0, 'f', 3).arg(inN).arg(tarN));
        inputNoteLabel->setText("Detected: " + inN);
        targetNoteLabel->setText("Target: " + tarN + " (Pure DSP)");
    }
    else {
        inputFreqLabel->setText("Input: Silence");
        correctedFreqLabel->setText("Corrected: Silence");
        ratioLabel->setText("Ratio: 1.000x | Waiting for mic signal");
        inputNoteLabel->setText("Detected: ---");
        targetNoteLabel->setText("Target: --- (Pure DSP)");
    }

    pitchGraph->addPoint(inF, tarF);
}

// ============================================================================
// SONG AUDIO DECODER & DSP ROUTING
// ============================================================================
void KaraokeDSPDashboard::setupAudioDecoder() {
    m_songDecoder = new QAudioDecoder(this);

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);

    m_songDecoder->setAudioFormat(format);

    connect(m_songDecoder, &QAudioDecoder::bufferReady, this, &KaraokeDSPDashboard::onDecoderBufferReady);
    connect(m_songDecoder, &QAudioDecoder::finished, this, [this]() {
        std::cout << "[DSP Engine] Song audio decoding completed and routed to AudioStream." << std::endl;
        });
}

void KaraokeDSPDashboard::setupVocalsDecoder() {
    m_vocalsDecoder = new QAudioDecoder(this);

    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    m_vocalsDecoder->setAudioFormat(format);

    connect(m_vocalsDecoder, &QAudioDecoder::bufferReady, this, &KaraokeDSPDashboard::onVocalsDecoderBufferReady);
    connect(m_vocalsDecoder, &QAudioDecoder::finished, this, [this]() {
        std::cout << "[VocalIsolation] Isolated vocals decoding complete. Samples collected: "
            << mVocalsSamplesForAnalysis.size() << std::endl;

        if (mTargetLoadedFromExistingCache) {
            std::cout << "[SongAnalyzer] Existing target JSON already loaded; skipping analysis/regeneration." << std::endl;
            return;
        }

        auto applyTimeline = [this](TargetPitchTimeline& analyzed, const char* src) {
            mTargetTimeline.Clear();
            for (const auto& frame : analyzed.GetFrames()) mTargetTimeline.AddPitchFrame(frame);
            size_t voiced = 0;
            for (const auto& f : mTargetTimeline.GetFramesRef()) if (f.isVoiced && f.targetHz > 0.0f) ++voiced;
            if (mAudioStream) {
                mAudioStream->SetTargetPitchTimeline(&mTargetTimeline);
                mAudioStream->EnableMelodyCorrection(true);
                mAudioStream->SetCorrectionStrength(1.0f);
            }
            mTargetLoadedFromExistingCache = true;
            std::cout << "[TargetDebug] Target applied from " << src << ": voiced=" << voiced
                << " / " << mTargetTimeline.GetFrameCount() << " frames; melody correction enabled." << std::endl;
        };

        // 1) Preferred: analyze the isolated vocal stem (cleanest pitch signal).
        if (!mVocalsSamplesForAnalysis.empty()) {
            TargetPitchTimeline analyzedTimeline;
            bool ok = SongAnalyzer::LoadOrAnalyzeSong(
                analyzedTimeline,
                mLoadedSongPath.toStdString(), // FIX: was mVocalsFilePath ("vocals.wav") — caused cache files
                                                // to be named "vocals_vocals_<hash>.json" instead of
                                                // "<SongName>_vocals_<hash>.json", making them indistinguishable
                                                // between different songs and triggering wrong-song cache loads.
                mVocalsSamplesForAnalysis,
                mVocalsSampleRateForAnalysis,
                true
            );
            if (ok) {
                std::cout << "[SongAnalyzer] Vocals-only pitch analysis succeeded for: "
                    << mLoadedSongPath.toStdString() << std::endl;
                applyTimeline(analyzedTimeline, "vocal stem");
                return;
            }
            std::cout << "[TargetDebug] Vocal-stem analysis produced no voiced target (silent/failed stem); "
                "falling back to full-song audio." << std::endl;
        }
        else {
            std::cout << "[TargetDebug] No vocal-stem samples decoded; falling back to full-song audio." << std::endl;
        }

        // 2) Fallback: analyze the full-song audio. Less clean than an isolated stem but produces REAL pitch
        //    data (never fabricated) and is far better than an empty target when demucs yields a silent stem.
        if (!mFullSongSamplesForAnalysis.empty()) {
            TargetPitchTimeline fullTimeline;
            bool ok2 = SongAnalyzer::LoadOrAnalyzeSong(
                fullTimeline,
                mLoadedSongPath.toStdString(),
                mFullSongSamplesForAnalysis,
                mSongSampleRateForAnalysis,
                false
            );
            if (ok2) {
                std::cout << "[SongAnalyzer] Full-song fallback analysis succeeded for: "
                    << mLoadedSongPath.toStdString() << std::endl;
                applyTimeline(fullTimeline, "full-song fallback");
                return;
            }
            std::cout << "[TargetDebug] Full-song fallback also produced no voiced target; "
                "no valid target available for this song." << std::endl;
        }
        else {
            std::cout << "[TargetDebug] No full-song samples available for fallback analysis." << std::endl;
        }
        });
}

void KaraokeDSPDashboard::setupSongDecoder(const QString& filePath) {
    if (m_songDecoder) {
        m_songDecoder->stop();
    }

    m_songDecoder->setSource(filePath);
    m_songDecoder->start();
}

void KaraokeDSPDashboard::onDecoderBufferReady() {
    if (!m_songDecoder || !mAudioStream) return;

    QAudioBuffer buffer = m_songDecoder->read();
    if (!buffer.isValid()) return;

    int sampleCount = buffer.sampleCount();
    int channelCount = buffer.format().channelCount();
    auto sampleFormat = buffer.format().sampleFormat();

    if (sampleCount <= 0) return;

    if (sampleFormat == QAudioFormat::Float) {
        const float* pcmData = buffer.constData<float>();
        if (!pcmData) return;

        mAudioStream->pushSongBuffer(pcmData, sampleCount);

        // PERFORMANCE FIX: mFullSongSamplesForAnalysis is only needed to feed a fresh
        // SongAnalyzer pass. When the target pitch was already loaded from an existing
        // cache file, no fresh analysis will run -- so accumulating every decoded buffer
        // into this ever-growing vector on the GUI thread, DURING live playback, was pure
        // wasted work competing with the 10ms audio timer on the same thread (a likely
        // cause of the periodic song-audio cracking/stutter).
        if (!mTargetLoadedFromExistingCache) {
        if (channelCount == 2) {
            int frameCount = sampleCount / 2;
            for (int i = 0; i < frameCount; ++i) {
                float monoSample = (pcmData[i * 2] + pcmData[i * 2 + 1]) * 0.5f;
                mFullSongSamplesForAnalysis.push_back(monoSample);
            }
        }
        else {
            mFullSongSamplesForAnalysis.insert(mFullSongSamplesForAnalysis.end(), pcmData, pcmData + sampleCount);
        }
        }
    }
    else if (sampleFormat == QAudioFormat::Int16) {
        const qint16* pcmData16 = buffer.constData<qint16>();
        if (!pcmData16) return;

        std::vector<float> tempFloat;
        tempFloat.reserve(sampleCount);
        for (int i = 0; i < sampleCount; ++i) {
            tempFloat.push_back(static_cast<float>(pcmData16[i]) / 32768.0f);
        }

        mAudioStream->pushSongBuffer(tempFloat.data(), sampleCount);

        if (!mTargetLoadedFromExistingCache) {
        if (channelCount == 2) {
            int frameCount = sampleCount / 2;
            for (int i = 0; i < frameCount; ++i) {
                float monoSample = (tempFloat[i * 2] + tempFloat[i * 2 + 1]) * 0.5f;
                mFullSongSamplesForAnalysis.push_back(monoSample);
            }
        }
        else {
            mFullSongSamplesForAnalysis.insert(mFullSongSamplesForAnalysis.end(), tempFloat.begin(), tempFloat.end());
        }
        }
    }
    else {
        std::cout << "[Decoder] Unsupported sample format or size; skipping buffer" << std::endl;
        return;
    }

    mSongSampleRateForAnalysis = buffer.format().sampleRate();
}

void KaraokeDSPDashboard::isolateVocalsFromSong(const QString& songPath) {
    if (mVocalIsolationInProgress) {
        std::cout << "[VocalIsolation] Already in progress, skipping" << std::endl;
        return;
    }

    QFileInfo fileInfo(songPath);
    QString baseName = fileInfo.completeBaseName();

    QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/demucs_output";
    QDir dir;
    if (!dir.exists(outputDir)) {
        dir.mkpath(outputDir);
    }

    mVocalsFilePath = outputDir + "/" + baseName + "/vocals.wav";

    if (QFile::exists(mVocalsFilePath)) {
        std::cout << "[VocalIsolation] Vocals file already exists: " << mVocalsFilePath.toStdString() << std::endl;
        analyzeVocalsFile(mVocalsFilePath);
        return;
    }

    std::cout << "[VocalIsolation] Separating vocals from: " << songPath.toStdString() << std::endl;
    std::cout << "[VocalIsolation] Running in background - playback is not affected..." << std::endl;

    mVocalIsolationInProgress = true;

    QString program = "python";
    QStringList arguments;
    arguments << "-m" << "demucs" << "--two-stems=vocals" << songPath << "-o" << outputDir;

    m_vocalIsolationProcess->start(program, arguments);
}

void KaraokeDSPDashboard::onVocalIsolationFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    mVocalIsolationInProgress = false;

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        std::cout << "[VocalIsolation] Done, vocals saved to: " << mVocalsFilePath.toStdString() << std::endl;

        if (QFile::exists(mVocalsFilePath)) {
            analyzeVocalsFile(mVocalsFilePath);
            return;
        }

        QString outputDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/demucs_output";
        QFileInfo origSong(mLoadedSongPath);
        QString baseName = origSong.completeBaseName();
        QString foundPath;
        QDirIterator it(outputDir, QStringList() << "vocals.wav", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString candidate = it.next();
            if (candidate.contains(baseName, Qt::CaseInsensitive)) {
                foundPath = candidate;
                break;
            }
            if (foundPath.isEmpty()) foundPath = candidate;
        }

        if (!foundPath.isEmpty() && QFile::exists(foundPath)) {
            mVocalsFilePath = foundPath;
            std::cout << "[VocalIsolation] Found vocals at: " << mVocalsFilePath.toStdString() << std::endl;
            analyzeVocalsFile(mVocalsFilePath);
        }
        else {
            std::cout << "[VocalIsolation] ERROR: vocals.wav not found at expected path" << std::endl;
        }
    }
    else {
        std::cout << "[VocalIsolation] ERROR: Demucs failed with exit code " << exitCode << std::endl;
        QString errorOutput = m_vocalIsolationProcess->readAllStandardError();
        std::cout << "[VocalIsolation] Error output: " << errorOutput.toStdString() << std::endl;
    }
}

void KaraokeDSPDashboard::onVocalIsolationErrorOccurred(QProcess::ProcessError error) {
    mVocalIsolationInProgress = false;
    std::cout << "[VocalIsolation] ERROR: Process error occurred: " << error << std::endl;
}

void KaraokeDSPDashboard::analyzeVocalsFile(const QString& vocalsPath) {
    if (!m_vocalsDecoder) return;

    mVocalsSamplesForAnalysis.clear();
    m_vocalsDecoder->setSource(vocalsPath);
    m_vocalsDecoder->start();
}

void KaraokeDSPDashboard::onVocalsDecoderBufferReady() {
    if (!m_vocalsDecoder) return;

    QAudioBuffer buffer = m_vocalsDecoder->read();
    if (!buffer.isValid()) return;
    int sampleCount = buffer.sampleCount();
    int channelCount = buffer.format().channelCount();
    auto sampleFormat = buffer.format().sampleFormat();

    if (sampleCount <= 0) return;

    if (sampleFormat == QAudioFormat::Float) {
        const float* pcmData = buffer.constData<float>();
        if (!pcmData) return;

        if (channelCount == 2) {
            int frameCount = sampleCount / 2;
            for (int i = 0; i < frameCount; ++i) {
                float monoSample = (pcmData[i * 2] + pcmData[i * 2 + 1]) * 0.5f;
                mVocalsSamplesForAnalysis.push_back(monoSample);
            }
        }
        else {
            mVocalsSamplesForAnalysis.insert(mVocalsSamplesForAnalysis.end(), pcmData, pcmData + sampleCount);
        }
    }
    else if (sampleFormat == QAudioFormat::Int16) {
        const qint16* pcmData16 = buffer.constData<qint16>();
        if (!pcmData16) return;

        if (channelCount == 2) {
            int frameCount = sampleCount / 2;
            for (int i = 0; i < frameCount; ++i) {
                float a = static_cast<float>(pcmData16[i * 2]) / 32768.0f;
                float b = static_cast<float>(pcmData16[i * 2 + 1]) / 32768.0f;
                mVocalsSamplesForAnalysis.push_back((a + b) * 0.5f);
            }
        }
        else {
            for (int i = 0; i < sampleCount; ++i) {
                mVocalsSamplesForAnalysis.push_back(static_cast<float>(pcmData16[i]) / 32768.0f);
            }
        }
    }
    else {
        std::cout << "[VocalsDecoder] Unsupported sample format or size; skipping buffer" << std::endl;
        return;
    }

    mVocalsSampleRateForAnalysis = buffer.format().sampleRate();
}