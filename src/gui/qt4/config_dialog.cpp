#include "stdafx.h"

#include <functional>
#include <array>
#include <map>
#include <vector>

#include "../../pervasives/pervasives.h"
#include "config_dialog.h"
#include "config_audioio.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt4 {

typedef std::function<QWidget *(config_context &)> wid_fun;

struct config_page_spec {
    const char * const category;
    const char * const name;
    wid_fun f;
};

typedef std::map<std::string, std::vector<config_page_spec *> > category_map;

#define MAKE_PAGE(HOOT) ([](config_context &context) { return new HOOT(context); })
#define MAKE_PAGE_IGNORE(HOOT) ([](config_context &) { return new HOOT(); })

class expanded_tree_item : public QTreeWidgetItem {
public:
    expanded_tree_item(QTreeWidget *parent) : QTreeWidgetItem(parent) {
        setExpanded(true);
    }

    expanded_tree_item(QTreeWidgetItem *parent) : QTreeWidgetItem(parent) {
        setExpanded(true);
    }

    void setExpanded(bool) {
        QTreeWidgetItem::setExpanded(true);
    }
};

class config_treeview : public QTreeWidget {
public:
    config_treeview() : QTreeWidget() {
        header()->setStretchLastSection(true);
        header()->hide();
        header()->setResizeMode(0, QHeaderView::ResizeToContents);
        setRootIsDecorated(false);
        setMinimumWidth(150);
        setMaximumWidth(150);
        this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    }
};

config_dialog::config_dialog(config_context &context, QWidget *parent) : QDialog(parent) {
    static config_page_spec config_dialog_layout[] = {
        { "Audio I/O", "root", MAKE_PAGE(config_audioio_main) },
        { "Audio I/O", "Channel Assignment", MAKE_PAGE_IGNORE(QProgressBar) },
        { "GUI", "root", MAKE_PAGE_IGNORE(QTextEdit) },
        { "GUI", "Keyboard", MAKE_PAGE_IGNORE(QTextEdit) },
        { "GUI", "Mouse", MAKE_PAGE_IGNORE(QTextEdit) }
    };

    auto vsplit = new QVBoxLayout(this);
    auto hsplit = new QHBoxLayout;
    auto button_hsplit = new QHBoxLayout;

    vsplit->setContentsMargins(3, 3, 3, 3);
    vsplit->addLayout(hsplit);
    vsplit->addLayout(button_hsplit);

    category_list  = new config_treeview();
    category_pager = new QStackedWidget();
    hsplit->addWidget(category_list);
    hsplit->addWidget(category_pager);

    auto add_button = [&](const char *caption) -> QPushButton * {
        auto widget = new QPushButton(caption);
        button_hsplit->addWidget(widget);
        return widget;
    };

    button_ok = add_button("&OK");
    button_cancel = add_button("&Cancel");
    button_apply = add_button("&Apply");
    button_hsplit->insertStretch(0, 100);

    category_map categories;

    for_each(config_dialog_layout, [&](config_page_spec &the_guy) {
        categories[the_guy.category].push_back(&the_guy);
    });

    auto add_page = [&](expanded_tree_item *item, wid_fun &f) {
        auto page_idx = category_pager->addWidget(f(context));
        item->setData(0, Qt::UserRole, page_idx);
    };

    auto add_category = [&](category_map::value_type &the_guy) {
        auto &page_list = the_guy.second;
        auto iter = page_list.begin();
        auto end  = page_list.end();
        if (iter != end) {
            auto category_root = new expanded_tree_item(category_list);
            auto page_spec = *iter;
            category_root->setText(0, page_spec->category);
            add_page(category_root, page_spec->f);
            ++iter;
            std::for_each(iter, end, [&](config_page_spec *page_spec) {
                auto category_child = new expanded_tree_item(category_root);
                category_child->setText(0, page_spec->name);
                add_page(category_child, page_spec->f);
            });
        }
    };

    for_each(categories, add_category);

    QObject::connect(category_list, SIGNAL(itemSelectionChanged()), this, SLOT(change_page()));
}

void config_dialog::change_page() {
    auto active = category_list->selectedItems();
    if (!active.isEmpty()) {
        auto page_idx = active.first()->data(0, Qt::UserRole).toInt();
        category_pager->setCurrentIndex(page_idx);
    }
}


}
}
}