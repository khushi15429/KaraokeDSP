#ifndef PITCHWIDGET_H
#define PITCHWIDGET_H

#include <QWidget>

class PitchShifter;
class QSlider;
class QLabel;

class PitchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PitchWidget(PitchShifter* pitchShifter, QWidget* parent = nullptr);
    ~PitchWidget();

signals:
    void pitchShiftChanged(float semitones);

private slots:
    void onSliderValueChanged(int value);

private:
    PitchShifter* m_pitchShifter;
    QSlider* m_pitchSlider;
    QLabel* m_titleLabel;
    QLabel* m_valueLabel;

    void setupUI();
    void setupConnections();
};

#endif // PITCHWIDGET_H