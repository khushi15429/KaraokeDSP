#ifndef PLAYERWIDGET_H
#define PLAYERWIDGET_H

#include <QWidget>

class PitchWidget;
class PitchShifter;

class PlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerWidget(QWidget* parent = nullptr);
    ~PlayerWidget();

private:
    PitchWidget* m_pitchWidget;
    PitchShifter* m_pitchShifter;
};

#endif // PLAYERWIDGET_H