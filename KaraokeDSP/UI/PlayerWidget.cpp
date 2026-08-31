#include "PlayerWidget.h"
#include "PitchWidget.h"
#include "PitchShifter.h"
#include <QVBoxLayout>

PlayerWidget::PlayerWidget(QWidget* parent)
    : QWidget(parent)
    , m_pitchShifter(nullptr)
    , m_pitchWidget(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    m_pitchWidget = new PitchWidget(m_pitchShifter, this);
    layout->addWidget(m_pitchWidget);
    setLayout(layout);
}

PlayerWidget::~PlayerWidget()
{
}