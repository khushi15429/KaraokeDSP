#include "PitchWidget.h"
#include "PitchShifter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>

PitchWidget::PitchWidget(PitchShifter* pitchShifter, QWidget* parent)
    : QWidget(parent)
    , m_pitchShifter(pitchShifter)
{
    setupUI();
    setupConnections();
}

PitchWidget::~PitchWidget()
{
}

void PitchWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    m_titleLabel = new QLabel("PITCH SHIFT CONTROL", this);

    QHBoxLayout* sliderLayout = new QHBoxLayout();

    m_pitchSlider = new QSlider(Qt::Horizontal, this);
    m_pitchSlider->setRange(-12, 12);
    m_pitchSlider->setValue(0);

    m_valueLabel = new QLabel("0 Semi", this);

    sliderLayout->addWidget(m_pitchSlider);
    sliderLayout->addWidget(m_valueLabel);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addLayout(sliderLayout);

    setLayout(mainLayout);
}

void PitchWidget::setupConnections()
{
    connect(m_pitchSlider, &QSlider::valueChanged, this, &PitchWidget::onSliderValueChanged);
}

void PitchWidget::onSliderValueChanged(int value)
{
    m_valueLabel->setText(QString("%1 Semi").arg(value));

    float semitones = static_cast<float>(value);

    if (m_pitchShifter) {
        // m_pitchShifter->setPitchShift(semitones);
    }

    emit pitchShiftChanged(semitones);
}