#include "stdafx.h"

#include "../../pervasives/pervasives.h"

#include "comment_view.h"
#include "../../legacy_soundlib/Sndfile.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {


comment_view::comment_view(CSoundFile *legacy_module)
    : QWidget(), comment_editor(new QPlainTextEdit),
      legacy_module(legacy_module), busy_loading_comments(false)
{
    auto layout = new QVBoxLayout();
    setLayout(layout);

    layout->addWidget(comment_editor);

    QFont font("Monospace");
    font.setStyleHint(QFont::TypeWriter);

    comment_editor->setPlainText("Yes");
    comment_editor->appendPlainText("5555Yes");
    comment_editor->setFont(font);
    comment_editor->setLineWrapMode(QPlainTextEdit::NoWrap);

    QObject::connect(
        comment_editor, SIGNAL(textChanged()),
        this, SLOT(legacy_update_module_comment())
    );
}

void comment_view::legacy_set_comments_from_module(bool allow_editing) {
    if (busy_loading_comments) {
        return;
    }

    busy_loading_comments = true;
    comment_editor->clear();
    comment_editor->setUndoRedoEnabled(false);

    if (legacy_module->m_lpszSongComments) {
        uint8_t bytestr[256];
        uint8_t curchar;
        uint8_t *iter = reinterpret_cast<uint8_t *>(
            legacy_module->m_lpszSongComments
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
                comment_editor->appendPlainText((char *)bytestr);
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

    comment_editor->moveCursor(QTextCursor::Start);
    comment_editor->setUndoRedoEnabled(true);
    comment_editor->setEnabled(true);
    comment_editor->setReadOnly(!allow_editing);
    busy_loading_comments = false;
}

void comment_view::legacy_update_module_comment() {
    if (busy_loading_comments) {
        return;
    }

    if (!legacy_module->GetModSpecifications().hasComments) {
        return;
    }

    uint8_t bytestr[256];
    uint8_t *old_comment = nullptr;

    if (legacy_module->m_lpszSongComments) {
        old_comment = reinterpret_cast<uint8_t *>(
            legacy_module->m_lpszSongComments
        );
        legacy_module->m_lpszSongComments = nullptr;
    }

    auto comment_lines = comment_editor->
                         toPlainText().split(QRegExp("(\\r\\n|\\r|\\n)"));

    auto strip_trailing_garbage = [](QString &line) {
        int idx = line.length() - 1;
        for (; idx >= 0 && line.at(idx).toAscii() <= ' '; --idx) { }
        line.truncate(idx + 1);
    };

    for_each(comment_lines, strip_trailing_garbage);

    auto raw_comment = comment_lines.join("\r").toAscii();
    auto raw_comment_length = raw_comment.length();

    if (raw_comment_length > 0) {
        legacy_module->m_lpszSongComments = reinterpret_cast<char *>(
            new uint8_t[raw_comment_length + 1]
        );
        std::copy_n(raw_comment.constData(), raw_comment_length + 1,
                    legacy_module->m_lpszSongComments);
    }

    if (old_comment) {
        delete[] old_comment;
    }
}

}
}
}