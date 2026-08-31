#ifndef MIXERWIDGET_H
#define MIXERWIDGET_H

#include <QWidget>

class MixerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MixerWidget(QWidget* parent = nullptr);
    ~MixerWidget();

private:
    void setupConnections();
};

#endif // MIXERWIDGET_H 