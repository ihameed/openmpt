#pragma once

#include <QtWidgets/QtWidgets>

namespace modplug {
namespace gui {
namespace qt5 {

class sample_osc_viz : public QWidget {
public:
    sample_osc_viz();

    void paintEvent(QPaintEvent *) override;
};

class sample_loop_editor : public QWidget {
    Q_OBJECT
public:
    sample_loop_editor();

private:
    QGroupBox box;
    QComboBox type;
    QSpinBox start;
    QSpinBox end;
};

class sample_editor : public QWidget {
    Q_OBJECT
public:
    sample_editor();

    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QSpinBox current_sample;
    QDoubleSpinBox zoom;

    QSpinBox default_volume;
    QSpinBox global_volume;
    QSpinBox default_pan;
    QSpinBox c5freq;
    QSpinBox transpose;
    QLineEdit name;
    QLineEdit filename;

    sample_loop_editor loop;
    sample_loop_editor susloop;
    QGroupBox vibratobox;
};



}
}
}
