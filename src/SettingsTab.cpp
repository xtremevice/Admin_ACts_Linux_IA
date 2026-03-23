#include "SettingsTab.h"
#include "AuthDialog.h"
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <map>

// Simple config file location
static const std::string CONFIG_DIR =
    std::string(getenv("HOME") ? getenv("HOME") : "/root") + "/.config/admin-acts-linux";
static const std::string CONFIG_FILE = CONFIG_DIR + "/settings.conf";

SettingsTab::SettingsTab(PackageManager& pm)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 12),
      m_pm(pm),
      m_frame_startup("Inicio del sistema"),
      m_startup_box(Gtk::ORIENTATION_VERTICAL, 4),
      m_frame_schedule("Horario de actualizaciones automáticas"),
      m_schedule_grid(),
      m_frame_repos("Repositorios"),
      m_repos_outer(Gtk::ORIENTATION_VERTICAL, 4),
      m_repos_toolbar(Gtk::ORIENTATION_HORIZONTAL, 6)
{
    build_ui();
}

void SettingsTab::build_ui() {
    set_border_width(12);
    set_spacing(10);

    // =====================================================================
    // Section 1: Autostart
    // =====================================================================
    m_autostart_check.set_label("Iniciar Admin Acts Linux al arrancar el sistema");
    m_autostart_check.set_active(m_pm.get_autostart());
    m_autostart_check.signal_toggled().connect(
        sigc::mem_fun(*this, &SettingsTab::on_autostart_toggled));

    m_startup_box.set_border_width(8);
    m_startup_box.pack_start(m_autostart_check, Gtk::PACK_SHRINK);

    auto* startup_desc = Gtk::manage(new Gtk::Label(
        "Cuando está habilitado, la aplicación se iniciará automáticamente al iniciar sesión."));
    startup_desc->set_halign(Gtk::ALIGN_START);
    startup_desc->set_line_wrap(true);
    startup_desc->get_style_context()->add_class("dim-label");
    m_startup_box.pack_start(*startup_desc, Gtk::PACK_SHRINK);

    m_frame_startup.add(m_startup_box);
    pack_start(m_frame_startup, Gtk::PACK_SHRINK);

    // =====================================================================
    // Section 2: Update schedule
    // =====================================================================
    m_schedule_grid.set_border_width(8);
    m_schedule_grid.set_row_spacing(6);
    m_schedule_grid.set_column_spacing(10);

    m_schedule_check.set_label("Habilitar actualizaciones automáticas");
    m_schedule_grid.attach(m_schedule_check, 0, 0, 4, 1);

    auto* from_lbl = Gtk::manage(new Gtk::Label("Desde:"));
    from_lbl->set_halign(Gtk::ALIGN_END);
    m_hour_from.set_range(0, 23);
    m_hour_from.set_increments(1, 1);
    m_hour_from.set_value(2);
    m_hour_from.set_wrap(true);
    m_hour_from.set_width_chars(3);

    auto* to_lbl = Gtk::manage(new Gtk::Label("Hasta:"));
    to_lbl->set_halign(Gtk::ALIGN_END);
    m_hour_to.set_range(0, 23);
    m_hour_to.set_increments(1, 1);
    m_hour_to.set_value(6);
    m_hour_to.set_wrap(true);
    m_hour_to.set_width_chars(3);

    auto* h_lbl1 = Gtk::manage(new Gtk::Label(":00 h"));
    auto* h_lbl2 = Gtk::manage(new Gtk::Label(":00 h"));

    m_schedule_grid.attach(*from_lbl, 0, 1, 1, 1);
    m_schedule_grid.attach(m_hour_from, 1, 1, 1, 1);
    m_schedule_grid.attach(*h_lbl1,    2, 1, 1, 1);
    m_schedule_grid.attach(*to_lbl,    3, 1, 1, 1);
    m_schedule_grid.attach(m_hour_to,  4, 1, 1, 1);
    m_schedule_grid.attach(*h_lbl2,    5, 1, 1, 1);

    m_schedule_desc.set_text("El sistema comprobará y aplicará actualizaciones dentro del rango horario seleccionado.");
    m_schedule_desc.set_halign(Gtk::ALIGN_START);
    m_schedule_desc.set_line_wrap(true);
    m_schedule_desc.get_style_context()->add_class("dim-label");
    m_schedule_grid.attach(m_schedule_desc, 0, 2, 6, 1);

    auto* save_btn = Gtk::manage(new Gtk::Button("Guardar configuración"));
    save_btn->set_image_from_icon_name("document-save");
    save_btn->set_always_show_image(true);
    save_btn->signal_clicked().connect(sigc::mem_fun(*this, &SettingsTab::save_schedule));
    m_schedule_grid.attach(*save_btn, 0, 3, 2, 1);

    m_frame_schedule.add(m_schedule_grid);
    pack_start(m_frame_schedule, Gtk::PACK_SHRINK);

    // Load persisted schedule settings
    {
        std::ifstream ifs(CONFIG_FILE);
        std::string key, val;
        while (ifs >> key >> val) {
            if (key == "schedule_enabled")   m_schedule_check.set_active(val == "1");
            if (key == "schedule_from")       m_hour_from.set_value(std::stod(val));
            if (key == "schedule_to")         m_hour_to.set_value(std::stod(val));
        }
    }

    // =====================================================================
    // Section 3: Repositories
    // =====================================================================
    m_repos_toolbar.set_border_width(4);

    m_refresh_repos_btn.set_label("Recargar");
    m_refresh_repos_btn.set_image_from_icon_name("view-refresh");
    m_refresh_repos_btn.set_always_show_image(true);
    m_refresh_repos_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsTab::load_repos));

    m_add_repo_btn.set_label("Agregar repositorio");
    m_add_repo_btn.set_image_from_icon_name("list-add");
    m_add_repo_btn.set_always_show_image(true);
    m_add_repo_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &SettingsTab::on_add_repo_clicked));

    m_repos_toolbar.pack_start(m_refresh_repos_btn, Gtk::PACK_SHRINK);
    m_repos_toolbar.pack_start(m_add_repo_btn,      Gtk::PACK_SHRINK);

    // Repo tree model (TreeStore so we can group by file)
    m_repo_model = Gtk::TreeStore::create(m_repo_cols);
    m_repos_tree.set_model(m_repo_model);

    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Activo"));
        auto* cell = Gtk::manage(new Gtk::CellRendererToggle());
        cell->set_activatable(true);
        cell->signal_toggled().connect(
            sigc::mem_fun(*this, &SettingsTab::on_repo_enabled_toggled));
        col->pack_start(*cell, false);
        col->add_attribute(*cell, "active", m_repo_cols.col_enabled);
        // Don't show toggle for group (file) rows
        col->set_cell_data_func(*cell, [this](Gtk::CellRenderer* r,
                                              const Gtk::TreeModel::iterator& it) {
            auto* toggle = dynamic_cast<Gtk::CellRendererToggle*>(r);
            if (!toggle) return;
            // Group rows have empty uri
            Glib::ustring uri = (*it)[m_repo_cols.col_uri];
            toggle->set_visible(!uri.empty());
        });
        m_repos_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Gestor"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_repo_cols.col_manager);
        col->set_resizable(true);
        m_repos_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Tipo"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_repo_cols.col_type);
        col->set_resizable(true);
        m_repos_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("URI"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        cell->property_ellipsize() = Pango::ELLIPSIZE_END;
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_repo_cols.col_uri);
        col->set_resizable(true);
        col->set_expand(true);
        col->set_min_width(180);
        m_repos_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Suite / Sección"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_repo_cols.col_suite);
        col->set_resizable(true);
        m_repos_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Componentes"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_repo_cols.col_components);
        col->set_resizable(true);
        m_repos_tree.append_column(*col);
    }

    m_repos_scrolled.add(m_repos_tree);
    m_repos_scrolled.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    m_repos_scrolled.set_min_content_height(250);

    m_repos_outer.set_border_width(4);
    m_repos_outer.pack_start(m_repos_toolbar, Gtk::PACK_SHRINK);
    m_repos_outer.pack_start(m_repos_scrolled, Gtk::PACK_EXPAND_WIDGET);

    m_frame_repos.add(m_repos_outer);
    pack_start(m_frame_repos, Gtk::PACK_EXPAND_WIDGET);

    show_all_children();
    load_repos();
}

void SettingsTab::refresh() {
    m_autostart_check.set_active(m_pm.get_autostart());
    load_repos();
}

void SettingsTab::load_repos() {
    m_repo_model->clear();
    m_repos = m_pm.list_repositories();

    // Group by file_path
    std::map<std::string, Gtk::TreeModel::iterator> file_groups;

    for (auto& r : m_repos) {
        // Create group row if not exists
        if (file_groups.find(r.file_path) == file_groups.end()) {
            auto group_iter = m_repo_model->append();
            auto group_row  = *group_iter;
            group_row[m_repo_cols.col_file]    = r.file_path;
            group_row[m_repo_cols.col_manager] = r.manager;
            group_row[m_repo_cols.col_uri]     = "";  // empty = group row
            group_row[m_repo_cols.col_enabled] = true;
            file_groups[r.file_path] = group_iter;
        }

        auto child_iter = m_repo_model->append(file_groups[r.file_path]->children());
        auto row = *child_iter;
        row[m_repo_cols.col_enabled]    = r.enabled;
        row[m_repo_cols.col_manager]    = r.manager;
        row[m_repo_cols.col_type]       = r.type;
        row[m_repo_cols.col_uri]        = r.uri;
        row[m_repo_cols.col_suite]      = r.suite;
        row[m_repo_cols.col_components] = r.components;
        row[m_repo_cols.col_file]       = r.file_path;
        row[m_repo_cols.col_line]       = r.line;
    }

    m_repos_tree.expand_all();
}

void SettingsTab::on_autostart_toggled() {
    bool enable = m_autostart_check.get_active();
    // Autostart doesn't need root (XDG autostart in user's home)
    bool ok = m_pm.set_autostart(enable, "");
    if (!ok) {
        m_autostart_check.set_active(!enable);  // revert
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        Gtk::MessageDialog dlg(top ? *top : *(Gtk::Window*)nullptr,
                               "No se pudo cambiar la configuración de inicio automático.",
                               false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dlg.run();
    }
}

void SettingsTab::on_repo_enabled_toggled(const Glib::ustring& path_str) {
    auto iter = m_repo_model->get_iter(path_str);
    if (!iter) return;

    bool currently_enabled = (*iter)[m_repo_cols.col_enabled];
    Glib::ustring line     = (*iter)[m_repo_cols.col_line];
    Glib::ustring file     = (*iter)[m_repo_cols.col_file];
    Glib::ustring uri      = (*iter)[m_repo_cols.col_uri];

    if (uri.empty()) return;  // group row

    std::string password;
    std::string action = currently_enabled ? "deshabilitar" : "habilitar";
    if (!ask_password("Se requieren permisos para " + action + " el repositorio:\n" + line.raw(),
                      password))
        return;

    // Find the matching Repository struct
    for (auto& r : m_repos) {
        if (r.line == line.raw() && r.file_path == file.raw()) {
            bool ok = false;
            if (currently_enabled)
                ok = m_pm.disable_repository(r, password);
            else
                ok = m_pm.enable_repository(r, password);

            if (ok)
                (*iter)[m_repo_cols.col_enabled] = !currently_enabled;
            else {
                Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
                Gtk::MessageDialog dlg(top ? *top : *(Gtk::Window*)nullptr,
                                       "Error al modificar el repositorio.",
                                       false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                dlg.run();
            }
            break;
        }
    }
}

void SettingsTab::on_add_repo_clicked() {
    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());

    // Dialog to add a new repo line
    Gtk::Dialog dlg("Agregar repositorio", top ? *top : *(Gtk::Window*)nullptr, true);
    dlg.set_default_size(520, -1);
    dlg.set_border_width(10);

    auto* content = dlg.get_content_area();
    content->set_spacing(8);

    auto* lbl = Gtk::manage(new Gtk::Label(
        "Ingrese la línea completa del repositorio (formato deb):\n"
        "Ejemplo: deb https://example.com/ubuntu focal main"));
    lbl->set_halign(Gtk::ALIGN_START);
    lbl->set_line_wrap(true);
    content->pack_start(*lbl, Gtk::PACK_SHRINK);

    auto* entry = Gtk::manage(new Gtk::Entry());
    entry->set_placeholder_text("deb https://... suite components");
    entry->set_hexpand(true);
    content->pack_start(*entry, Gtk::PACK_SHRINK);

    auto* file_lbl = Gtk::manage(new Gtk::Label("Archivo destino:"));
    file_lbl->set_halign(Gtk::ALIGN_START);
    content->pack_start(*file_lbl, Gtk::PACK_SHRINK);

    auto* file_entry = Gtk::manage(new Gtk::Entry());
    file_entry->set_text("/etc/apt/sources.list.d/custom.list");
    file_entry->set_hexpand(true);
    content->pack_start(*file_entry, Gtk::PACK_SHRINK);

    dlg.add_button("Cancelar", Gtk::RESPONSE_CANCEL);
    dlg.add_button("Agregar",  Gtk::RESPONSE_OK);
    dlg.set_default_response(Gtk::RESPONSE_OK);
    dlg.show_all_children();

    if (dlg.run() != Gtk::RESPONSE_OK) return;

    std::string new_line  = entry->get_text();
    std::string dest_file = file_entry->get_text();
    if (new_line.empty() || dest_file.empty()) return;

    std::string password;
    if (!ask_password("Agregar repositorio al archivo " + dest_file, password))
        return;

    bool ok = m_pm.add_repository(new_line, dest_file, password);
    if (ok) {
        load_repos();
    } else {
        Gtk::MessageDialog err_dlg(top ? *top : *(Gtk::Window*)nullptr,
                                   "Error al agregar el repositorio.",
                                   false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        err_dlg.run();
    }
}

bool SettingsTab::ask_password(const std::string& reason, std::string& out_password) {
    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!top) return false;
    AuthDialog dlg(*top, reason);
    if (dlg.run() != Gtk::RESPONSE_OK) return false;
    out_password = dlg.get_password();
    return true;
}

void SettingsTab::save_schedule() {
    // Persist settings to config file
    std::filesystem::create_directories(CONFIG_DIR);
    std::ofstream ofs(CONFIG_FILE);
    if (!ofs) {
        Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
        Gtk::MessageDialog dlg(top ? *top : *(Gtk::Window*)nullptr,
                               "No se pudo guardar la configuración.",
                               false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dlg.run();
        return;
    }
    ofs << "schedule_enabled " << (m_schedule_check.get_active() ? "1" : "0") << "\n";
    ofs << "schedule_from "    << (int)m_hour_from.get_value() << "\n";
    ofs << "schedule_to "      << (int)m_hour_to.get_value()   << "\n";

    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    Gtk::MessageDialog dlg(top ? *top : *(Gtk::Window*)nullptr,
                           "Configuración guardada correctamente.",
                           false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
    dlg.run();
}
