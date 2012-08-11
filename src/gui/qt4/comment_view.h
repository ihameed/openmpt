#pragma once

#include <QtGui>

class CSoundFile;

namespace modplug {
namespace gui {
namespace qt4 {

static const unsigned int max_comment_line_length = 128;

class comment_view : public QWidget {
    Q_OBJECT
public:
    comment_view(CSoundFile *);

    void legacy_set_comments_from_module(bool);

public slots:
    void legacy_update_module_comment();

private:
    QPlainTextEdit comment_editor;
    CSoundFile *legacy_module;
    bool busy_loading_comments;
};

}
}
}