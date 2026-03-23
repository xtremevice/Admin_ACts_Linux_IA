#pragma once

#include <gtkmm.h>
#include <memory>
#include "PackageManager.h"

// Tab 2 – Install packages
// Allows users to search and install packages from all available managers.
class InstallTab : public Gtk::Box {
public:
    explicit InstallTab(PackageManager& pm);
    ~InstallTab() override { *m_alive = false; }

private:
    PackageManager& m_pm;

    // Search bar
    Gtk::Box       m_search_box;
    Gtk::Entry     m_search_entry;
    Gtk::Button    m_search_btn;
    Gtk::Spinner   m_spinner;
    Gtk::Label     m_status_label;

    // Results list
    Gtk::ScrolledWindow m_scrolled;
    Gtk::TreeView  m_tree;

    struct Columns : public Gtk::TreeModel::ColumnRecord {
        Columns() {
            add(col_name);
            add(col_version);
            add(col_desc);
            add(col_manager);
            add(col_installed);
        }
        Gtk::TreeModelColumn<Glib::ustring> col_name;
        Gtk::TreeModelColumn<Glib::ustring> col_version;
        Gtk::TreeModelColumn<Glib::ustring> col_desc;
        Gtk::TreeModelColumn<Glib::ustring> col_manager;
        Gtk::TreeModelColumn<bool>          col_installed;
    };
    Columns m_columns;
    Glib::RefPtr<Gtk::ListStore> m_model;

    // Alive flag for safe detached thread callbacks
    std::shared_ptr<bool> m_alive;

    void build_ui();
    void on_search_clicked();
    void on_install_clicked(const Glib::ustring& name, const Glib::ustring& manager);
    bool ask_password(const std::string& reason, std::string& out_password);
    void set_busy(bool busy);
};
