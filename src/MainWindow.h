#pragma once

#include <gtkmm.h>
#include <memory>

#include "PackageManager.h"
#include "UpdatesTab.h"
#include "InstallTab.h"
#include "SettingsTab.h"

// The main application window containing three tabs.
class MainWindow : public Gtk::Window {
public:
    MainWindow();
    ~MainWindow() override = default;

private:
    PackageManager m_pm;

    Gtk::Box      m_main_box;
    Gtk::Notebook m_notebook;

    // Tab widgets
    std::unique_ptr<UpdatesTab>  m_updates_tab;
    std::unique_ptr<InstallTab>  m_install_tab;
    std::unique_ptr<SettingsTab> m_settings_tab;

    // Header bar
    Gtk::HeaderBar m_header;

    void build_ui();
    void on_tab_switched(Gtk::Widget* page, guint page_num);
};
