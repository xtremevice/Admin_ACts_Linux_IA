#include "InstallTab.h"
#include "AuthDialog.h"
#include <thread>

InstallTab::InstallTab(PackageManager& pm)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0),
      m_pm(pm),
      m_search_box(Gtk::ORIENTATION_HORIZONTAL, 6)
{
    build_ui();
}

void InstallTab::build_ui() {
    set_border_width(8);

    // ---- Search bar ----
    m_search_box.set_border_width(4);

    auto* lbl = Gtk::manage(new Gtk::Label("Buscar paquete:"));
    m_search_entry.set_placeholder_text("Nombre o descripción del paquete...");
    m_search_entry.set_hexpand(true);
    m_search_entry.set_activates_default(false);
    m_search_entry.signal_activate().connect(
        sigc::mem_fun(*this, &InstallTab::on_search_clicked));

    m_search_btn.set_label("Buscar");
    m_search_btn.set_image_from_icon_name("system-search");
    m_search_btn.set_always_show_image(true);
    m_search_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &InstallTab::on_search_clicked));

    m_spinner.set_size_request(24, 24);
    m_status_label.set_text("Ingrese un término de búsqueda");
    m_status_label.set_halign(Gtk::ALIGN_START);
    m_status_label.set_hexpand(true);

    m_search_box.pack_start(*lbl,          Gtk::PACK_SHRINK);
    m_search_box.pack_start(m_search_entry, Gtk::PACK_EXPAND_WIDGET);
    m_search_box.pack_start(m_search_btn,  Gtk::PACK_SHRINK);
    m_search_box.pack_start(m_spinner,     Gtk::PACK_SHRINK);

    pack_start(m_search_box, Gtk::PACK_SHRINK);

    auto* status_row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
    status_row->set_border_width(4);
    status_row->pack_start(m_status_label, Gtk::PACK_EXPAND_WIDGET);
    pack_start(*status_row, Gtk::PACK_SHRINK);

    auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    pack_start(*sep, Gtk::PACK_SHRINK);

    // ---- Tree model ----
    m_model = Gtk::ListStore::create(m_columns);

    // ---- Tree view ----
    m_tree.set_model(m_model);
    m_tree.set_rules_hint(true);

    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Nombre"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_name);
        col->set_sort_column(m_columns.col_name);
        col->set_resizable(true);
        col->set_min_width(140);
        m_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Última versión"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_version);
        col->set_resizable(true);
        m_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Descripción / Funciones"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        cell->property_ellipsize() = Pango::ELLIPSIZE_END;
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_desc);
        col->set_resizable(true);
        col->set_expand(true);
        col->set_min_width(200);
        m_tree.append_column(*col);
    }
    {
        auto* col  = Gtk::manage(new Gtk::TreeViewColumn("Gestor"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_manager);
        col->set_resizable(true);
        m_tree.append_column(*col);
    }

    // Install / Already installed button column
    {
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Acción"));
        auto* cell = Gtk::manage(new Gtk::CellRendererPixbuf());
        cell->property_icon_name() = "list-add";
        cell->property_mode()      = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
        col->pack_start(*cell, true);
        // Show different icon if already installed
        col->set_cell_data_func(*cell, [this](Gtk::CellRenderer* renderer,
                                              const Gtk::TreeModel::iterator& iter) {
            auto* r = dynamic_cast<Gtk::CellRendererPixbuf*>(renderer);
            if (!r) return;
            bool installed = (*iter)[m_columns.col_installed];
            r->property_icon_name() = installed ? "object-select-symbolic" : "list-add";
            r->property_sensitive()  = !installed;
        });
        m_tree.append_column(*col);

        m_tree.signal_button_press_event().connect([this, col](GdkEventButton* ev) -> bool {
            if (ev->type != GDK_BUTTON_PRESS || ev->button != 1)
                return false;
            Gtk::TreePath path;
            Gtk::TreeViewColumn* clicked_col = nullptr;
            int cx = 0, cy = 0;
            if (!m_tree.get_path_at_pos((int)ev->x, (int)ev->y, path, clicked_col, cx, cy))
                return false;
            if (clicked_col != col) return false;

            auto iter = m_model->get_iter(path);
            if (!iter) return false;
            bool installed = (*iter)[m_columns.col_installed];
            if (installed) return true;
            Glib::ustring name    = (*iter)[m_columns.col_name];
            Glib::ustring manager = (*iter)[m_columns.col_manager];
            on_install_clicked(name, manager);
            return true;
        }, false);
    }

    m_scrolled.add(m_tree);
    m_scrolled.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    pack_start(m_scrolled, Gtk::PACK_EXPAND_WIDGET);

    show_all_children();
}

// ---------------------------------------------------------------------------

void InstallTab::on_search_clicked() {
    Glib::ustring query = m_search_entry.get_text();
    if (query.empty()) {
        m_status_label.set_text("Ingrese un término de búsqueda");
        return;
    }

    set_busy(true);
    m_status_label.set_text("Buscando «" + query + "»...");
    m_model->clear();

    std::thread([this, query = query.raw()]() {
        auto results = m_pm.search(query);
        Glib::signal_idle().connect_once([this, results = std::move(results)]() mutable {
            m_model->clear();
            for (auto& p : results) {
                auto row = *m_model->append();
                row[m_columns.col_name]      = p.name;
                row[m_columns.col_version]   = p.latest_version;
                row[m_columns.col_desc]      = p.description;
                row[m_columns.col_manager]   = p.manager;
                row[m_columns.col_installed] = !p.current_version.empty();
            }
            set_busy(false);
            m_status_label.set_text(std::to_string(results.size()) + " resultado(s) encontrado(s)");
        });
    }).detach();
}

bool InstallTab::ask_password(const std::string& reason, std::string& out_password) {
    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!top) return false;
    AuthDialog dlg(*top, reason);
    if (dlg.run() != Gtk::RESPONSE_OK) return false;
    out_password = dlg.get_password();
    return true;
}

void InstallTab::on_install_clicked(const Glib::ustring& name, const Glib::ustring& /*manager*/) {
    std::string password;
    if (!ask_password("Instalar paquete: " + name, password))
        return;

    set_busy(true);
    m_status_label.set_text("Instalando " + name + " con todas sus dependencias...");

    std::thread([this, name = name.raw(), password]() mutable {
        bool ok = m_pm.install_package(name, password, [this](const std::string& msg) {
            Glib::signal_idle().connect_once([this, msg]() {
                m_status_label.set_text(msg.substr(0, 120));
            });
        });

        Glib::signal_idle().connect_once([this, ok, name]() {
            set_busy(false);
            if (ok) {
                m_status_label.set_text(name + " instalado correctamente con todas sus dependencias.");
                // Mark as installed in the list
                for (auto& row : m_model->children()) {
                    Glib::ustring row_name = row[m_columns.col_name];
                    if (row_name == name) {
                        row[m_columns.col_installed] = true;
                        break;
                    }
                }
            } else {
                m_status_label.set_text("Error al instalar " + name + ".");
            }
        });
    }).detach();
}

void InstallTab::set_busy(bool busy) {
    if (busy) {
        m_spinner.start();
        m_search_btn.set_sensitive(false);
        m_search_entry.set_sensitive(false);
    } else {
        m_spinner.stop();
        m_search_btn.set_sensitive(true);
        m_search_entry.set_sensitive(true);
    }
}
