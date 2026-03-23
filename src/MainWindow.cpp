#include "MainWindow.h"

MainWindow::MainWindow()
    : m_main_box(Gtk::ORIENTATION_VERTICAL, 0)
{
    build_ui();
}

void MainWindow::build_ui() {
    // ---- Header bar ----
    m_header.set_title("Admin Acts Linux");
    m_header.set_subtitle("Gestor de paquetes");
    m_header.set_show_close_button(true);

    // App icon in header
    auto* icon = Gtk::manage(new Gtk::Image());
    icon->set_from_icon_name("system-software-update", Gtk::ICON_SIZE_LARGE_TOOLBAR);
    m_header.pack_start(*icon);

    set_titlebar(m_header);
    set_default_size(1100, 750);
    set_position(Gtk::WIN_POS_CENTER);

    // ---- Notebook (tabs) ----
    m_updates_tab  = std::make_unique<UpdatesTab>(m_pm);
    m_install_tab  = std::make_unique<InstallTab>(m_pm);
    m_settings_tab = std::make_unique<SettingsTab>(m_pm);

    // Tab labels with icons
    auto make_tab_label = [](const std::string& icon_name, const std::string& text) {
        auto* box  = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        auto* icon = Gtk::manage(new Gtk::Image());
        icon->set_from_icon_name(icon_name, Gtk::ICON_SIZE_MENU);
        auto* lbl  = Gtk::manage(new Gtk::Label(text));
        box->pack_start(*icon, Gtk::PACK_SHRINK);
        box->pack_start(*lbl,  Gtk::PACK_SHRINK);
        box->show_all();
        return box;
    };

    m_notebook.append_page(*m_updates_tab,
        *make_tab_label("system-software-update", "Actualizaciones"));
    m_notebook.append_page(*m_install_tab,
        *make_tab_label("list-add", "Instalar"));
    m_notebook.append_page(*m_settings_tab,
        *make_tab_label("preferences-system", "Configuración"));

    m_notebook.signal_switch_page().connect(
        sigc::mem_fun(*this, &MainWindow::on_tab_switched));

    m_main_box.pack_start(m_notebook, Gtk::PACK_EXPAND_WIDGET);
    add(m_main_box);
    show_all_children();
}

void MainWindow::on_tab_switched(Gtk::Widget* /*page*/, guint page_num) {
    switch (page_num) {
    case 0:
        // Updates tab – refresh only if empty
        break;
    case 2:
        m_settings_tab->refresh();
        break;
    default:
        break;
    }
}
