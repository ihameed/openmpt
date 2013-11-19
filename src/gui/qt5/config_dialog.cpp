#include "stdafx.h"

#include <functional>
#include <array>
#include <map>
#include <vector>

#include "../../pervasives/pervasives.hpp"
#include "config_dialog.hpp"
#include "config_audioio.hpp"
#include "config_gui.hpp"
#include "../../MainFrm.h"

using namespace modplug::pervasives;

namespace modplug {
namespace gui {
namespace qt5 {

typedef std::function<config_page *(app_config &)> wid_fun;

struct config_page_spec {
    const char * const category;
    const char * const name;
    wid_fun f;
};

typedef std::map<std::string, std::vector<config_page_spec *> > category_map;

#define MAKE_PAGE(HOOT) \
    ([](app_config &context) -> config_page* { return new HOOT(context); })
#define MAKE_PAGE_IGNORE(HOOT) \
    ([](app_config &) -> config_page* { return new HOOT(); })

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
        header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        setRootIsDecorated(false);
        setMinimumWidth(150);
        setMaximumWidth(150);
        this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    }
};

class empty_config_page : public config_page {
    virtual void refresh() { }
    virtual void apply_changes() { }
};

config_dialog::config_dialog(app_config &context, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Preferences");
    static config_page_spec config_dialog_layout[] = {
        { "Audio I/O", "root", MAKE_PAGE(config_audioio_main) },
        { "Audio I/O", "Channel Assignment", MAKE_PAGE_IGNORE(empty_config_page) },
        { "GUI", "root", MAKE_PAGE(config_gui_main) },
        { "GUI", "Keyboard", MAKE_PAGE_IGNORE(empty_config_page) },
        { "GUI", "Mouse", MAKE_PAGE_IGNORE(empty_config_page) },
        { "Plugins", "root", MAKE_PAGE_IGNORE(empty_config_page) },
        { "Plugins", "VST2", MAKE_PAGE_IGNORE(empty_config_page) },
    };

    auto vsplit = new QVBoxLayout(this);
    auto hsplit = new QHBoxLayout;

    vsplit->setContentsMargins(3, 3, 3, 3);
    vsplit->addLayout(hsplit);

    buttons.addButton("&OK", QDialogButtonBox::AcceptRole);
    buttons.addButton("&Cancel", QDialogButtonBox::RejectRole);
    buttons.addButton("&Apply", QDialogButtonBox::ApplyRole);
    vsplit->addWidget(&buttons);

    category_list  = new config_treeview();
    category_pager = new QStackedWidget();
    hsplit->addWidget(category_list);
    hsplit->addWidget(category_pager);

    category_map categories;

    for_each(config_dialog_layout, [&](config_page_spec &the_guy) {
        categories[the_guy.category].push_back(&the_guy);
    });

    auto add_page = [&](expanded_tree_item *item, wid_fun &f) {
        auto page = f(context);
        auto page_idx = category_pager->addWidget(page);
        item->setData(0, Qt::UserRole, page_idx);
        pages.push_back(page);
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

    connect(
        category_list, &config_treeview::itemSelectionChanged,
        this, &config_dialog::change_page
    );

    connect(
        &buttons, &QDialogButtonBox::clicked,
        this, &config_dialog::button_clicked
    );
}

void config_dialog::change_page() {
    auto active = category_list->selectedItems();
    if (!active.isEmpty()) {
        auto page_idx = active.first()->data(0, Qt::UserRole).toInt();
        category_pager->setCurrentIndex(page_idx);
    }
}

void config_dialog::button_clicked(QAbstractButton *button) {
    switch (buttons.buttonRole(button)) {
    case QDialogButtonBox::AcceptRole:
        hide();
    case QDialogButtonBox::ApplyRole:
        for_each(pages, [] (config_page *page) {
            page->apply_changes();
        });
        break;
    case QDialogButtonBox::RejectRole:
        hide();
        break;
    }
}

void config_dialog::setVisible(bool ya) {
    if (ya) {
        for_each(pages, [] (config_page *page) {
            page->refresh();
        });
    }
    QDialog::setVisible(ya);
}


}
}
}
