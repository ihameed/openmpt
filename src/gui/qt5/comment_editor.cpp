#include "stdafx.h"

#include "../../pervasives/pervasives.hpp"

#include "comment_editor.hpp"
#include "../../legacy_soundlib/Sndfile.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt5 {


comment_editor::comment_editor(module_renderer &legacy_module)
    : QWidget(), legacy_module(legacy_module), busy_loading_comments(false)
{
    auto layout = new QVBoxLayout();
    setLayout(layout);

    layout->addWidget(&editor);

    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);

    editor.setPlainText("Yes");
    editor.appendPlainText("5555Yes");
    editor.setFont(font);
    editor.setLineWrapMode(QPlainTextEdit::NoWrap);

    connect(
        &editor, &QPlainTextEdit::textChanged,
        this, &comment_editor::legacy_update_module_comment
    );
}

void comment_editor::legacy_set_comments_from_module(bool allow_editing) {
    if (busy_loading_comments) {
        return;
    }

    busy_loading_comments = true;
    editor.clear();
    editor.setUndoRedoEnabled(false);

    if (legacy_module.m_lpszSongComments) {
        uint8_t bytestr[256];
        uint8_t curchar;
        uint8_t *iter = reinterpret_cast<uint8_t *>(
            legacy_module.m_lpszSongComments
        );
        unsigned int line_length = 0;

        while ( (curchar = *iter++) != '\0' ) {
            bool at_end_of_line = line_length >= max_comment_line_length - 1;
            bool at_end_of_iter = *iter == '\0';

            if (at_end_of_line || at_end_of_iter) {
                if (curchar > ' ') {
                    bytestr[line_length++] = curchar;
                }
                curchar = '\r';
            }

            if (curchar == '\r') {
                bytestr[line_length] = '\0';
                editor.appendPlainText((char *)bytestr);
                bytestr[0] = '\0';
                line_length = 0;
            } else {
                if (curchar < ' ') {
                    curchar = ' ';
                }
                bytestr[line_length++] = curchar;
            }
        }
    }

    editor.moveCursor(QTextCursor::Start);
    editor.setUndoRedoEnabled(true);
    editor.setEnabled(true);
    editor.setReadOnly(!allow_editing);
    busy_loading_comments = false;
}

void comment_editor::legacy_update_module_comment() {
    if (busy_loading_comments) {
        return;
    }

    if (!legacy_module.GetModSpecifications().hasComments) {
        return;
    }

    uint8_t *old_comment = nullptr;

    if (legacy_module.m_lpszSongComments) {
        old_comment = reinterpret_cast<uint8_t *>(
            legacy_module.m_lpszSongComments
        );
        legacy_module.m_lpszSongComments = nullptr;
    }

    auto comment_lines = editor.
                         toPlainText().split(QRegExp("(\\r\\n|\\r|\\n)"));

    auto strip_trailing_garbage = [](QString &line) {
        int idx = line.length() - 1;
        for (; idx >= 0 && line.at(idx).toLatin1() <= ' '; --idx) { }
        line.truncate(idx + 1);
    };

    for_each(comment_lines, strip_trailing_garbage);

    auto raw_comment = comment_lines.join("\r").toLatin1();
    auto raw_comment_length = raw_comment.length();

    if (raw_comment_length > 0) {
        legacy_module.m_lpszSongComments = reinterpret_cast<char *>(
            new uint8_t[raw_comment_length + 1]
        );
        std::copy_n(raw_comment.constData(), raw_comment_length + 1,
                    legacy_module.m_lpszSongComments);
    }

    if (old_comment) {
        delete[] old_comment;
    }
}

}
}
}
