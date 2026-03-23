#pragma once

#include <gtkmm.h>
#include <memory>
#include "PackageManager.h"

// Tab 1 – Updates
// Shows all installed packages with update/remove actions.
class UpdatesTab : public Gtk::Box {
public:
    explicit UpdatesTab(PackageManager& pm);
    ~UpdatesTab() override { *m_alive = false; }

    // Called by the main window to refresh when tab is activated
    void refresh();

private:
    PackageManager& m_pm;

    // Toolbar
    Gtk::Box       m_toolbar;
    Gtk::Button    m_refresh_btn;
    Gtk::Button    m_update_all_btn;
    Gtk::Spinner   m_spinner;
    Gtk::Label     m_status_label;

    // Filter / search
    Gtk::Box       m_filter_box;
    Gtk::SearchEntry m_search_entry;
    Gtk::ComboBoxText m_filter_combo;

    // Tree view
    Gtk::ScrolledWindow m_scrolled;
    Gtk::TreeView  m_tree;

    // Model columns
    struct Columns : public Gtk::TreeModel::ColumnRecord {
        Columns() {
            add(col_name);
            add(col_desc);
            add(col_size);
            add(col_current_ver);
            add(col_latest_ver);
            add(col_manager);
            add(col_has_update);
        }
        Gtk::TreeModelColumn<Glib::ustring> col_name;
        Gtk::TreeModelColumn<Glib::ustring> col_desc;
        Gtk::TreeModelColumn<Glib::ustring> col_size;
        Gtk::TreeModelColumn<Glib::ustring> col_current_ver;
        Gtk::TreeModelColumn<Glib::ustring> col_latest_ver;
        Gtk::TreeModelColumn<Glib::ustring> col_manager;
        Gtk::TreeModelColumn<bool>          col_has_update;
    };
    Columns m_columns;
    Glib::RefPtr<Gtk::ListStore>       m_model;
    Glib::RefPtr<Gtk::TreeModelFilter> m_filter_model;
    Glib::RefPtr<Gtk::TreeModelSort>   m_sort_model;

    // Cached packages
    std::vector<Package> m_packages;
    bool m_show_only_updates = false;

    // Shared alive flag: set to false when the widget is destroyed.
    // Background threads check this before accessing 'this'.
    std::shared_ptr<bool> m_alive;

    void build_ui();
    void populate(const std::vector<Package>& pkgs);
    void on_refresh_clicked();
    void on_update_all_clicked();
    void on_search_changed();
    void on_filter_changed();
    bool filter_func(const Gtk::TreeModel::const_iterator& iter);

    // Button callbacks: row actions
    void on_update_clicked(const Glib::ustring& name, const Glib::ustring& manager);
    void on_remove_clicked(const Glib::ustring& name, const Glib::ustring& manager);

    bool ask_password(const std::string& reason, std::string& out_password);
    void show_operation_dialog(const std::string& title,
                               const std::string& name,
                               bool               is_update);
    void set_busy(bool busy);
};
