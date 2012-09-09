#pragma once

#include <QtGui>

class module_renderer;

namespace modplug {
namespace gui {
namespace qt4 {

static const unsigned int max_comment_line_length = 128;

class comment_editor : public QWidget {
    Q_OBJECT
public:
    comment_editor(module_renderer &);

    void legacy_set_comments_from_module(bool);

public slots:
    void legacy_update_module_comment();

private:
    QPlainTextEdit editor;
    module_renderer &legacy_module;
    bool busy_loading_comments;
};

}
}
}