#pragma once

#include <gtkmm.h>
#include "PackageManager.h"

// Tab 3 – Settings / Configuration
class SettingsTab : public Gtk::Box {
public:
    explicit SettingsTab(PackageManager& pm);
    ~SettingsTab() override = default;

    void refresh();

private:
    PackageManager& m_pm;

    // ---- Autostart ----
    Gtk::Frame        m_frame_startup;
    Gtk::Box          m_startup_box;
    Gtk::CheckButton  m_autostart_check;

    // ---- Update schedule ----
    Gtk::Frame        m_frame_schedule;
    Gtk::Grid         m_schedule_grid;
    Gtk::CheckButton  m_schedule_check;
    Gtk::SpinButton   m_hour_from;
    Gtk::SpinButton   m_hour_to;
    Gtk::Label        m_schedule_desc;

    // ---- Repositories ----
    Gtk::Frame        m_frame_repos;
    Gtk::Box          m_repos_outer;
    Gtk::Box          m_repos_toolbar;
    Gtk::Button       m_add_repo_btn;
    Gtk::Button       m_refresh_repos_btn;
    Gtk::ScrolledWindow m_repos_scrolled;
    Gtk::TreeView     m_repos_tree;

    struct RepoColumns : public Gtk::TreeModel::ColumnRecord {
        RepoColumns() {
            add(col_enabled);
            add(col_manager);
            add(col_type);
            add(col_uri);
            add(col_suite);
            add(col_components);
            add(col_file);
            add(col_line);
        }
        Gtk::TreeModelColumn<bool>          col_enabled;
        Gtk::TreeModelColumn<Glib::ustring> col_manager;
        Gtk::TreeModelColumn<Glib::ustring> col_type;
        Gtk::TreeModelColumn<Glib::ustring> col_uri;
        Gtk::TreeModelColumn<Glib::ustring> col_suite;
        Gtk::TreeModelColumn<Glib::ustring> col_components;
        Gtk::TreeModelColumn<Glib::ustring> col_file;
        Gtk::TreeModelColumn<Glib::ustring> col_line;
    };
    RepoColumns m_repo_cols;
    Glib::RefPtr<Gtk::TreeStore> m_repo_model;   // grouped by file path

    std::vector<Repository> m_repos;

    void build_ui();
    void load_repos();
    void on_autostart_toggled();
    void on_repo_enabled_toggled(const Glib::ustring& path);
    void on_add_repo_clicked();
    bool ask_password(const std::string& reason, std::string& out_password);
    void save_schedule();
};
