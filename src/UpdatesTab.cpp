#include "UpdatesTab.h"
#include "AuthDialog.h"
#include <glibmm/main.h>
#include <thread>
#include <mutex>

UpdatesTab::UpdatesTab(PackageManager& pm)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0),
      m_pm(pm),
      m_toolbar(Gtk::ORIENTATION_HORIZONTAL, 6),
      m_filter_box(Gtk::ORIENTATION_HORIZONTAL, 6)
{
    build_ui();
}

void UpdatesTab::build_ui() {
    set_border_width(8);

    // ---- Toolbar ----
    m_toolbar.set_border_width(4);

    m_refresh_btn.set_label("Actualizar lista");
    m_refresh_btn.set_image_from_icon_name("view-refresh");
    m_refresh_btn.set_always_show_image(true);
    m_refresh_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &UpdatesTab::on_refresh_clicked));

    m_update_all_btn.set_label("Actualizar todo");
    m_update_all_btn.set_image_from_icon_name("system-software-update");
    m_update_all_btn.set_always_show_image(true);
    m_update_all_btn.get_style_context()->add_class("suggested-action");
    m_update_all_btn.signal_clicked().connect(
        sigc::mem_fun(*this, &UpdatesTab::on_update_all_clicked));

    m_spinner.set_size_request(24, 24);
    m_status_label.set_text("Listo");
    m_status_label.set_halign(Gtk::ALIGN_START);
    m_status_label.set_hexpand(true);

    m_toolbar.pack_start(m_refresh_btn,    Gtk::PACK_SHRINK);
    m_toolbar.pack_start(m_update_all_btn, Gtk::PACK_SHRINK);
    m_toolbar.pack_start(m_spinner,        Gtk::PACK_SHRINK);
    m_toolbar.pack_start(m_status_label,   Gtk::PACK_EXPAND_WIDGET);

    pack_start(m_toolbar, Gtk::PACK_SHRINK);

    // ---- Filter bar ----
    m_filter_box.set_border_width(4);

    m_search_entry.set_placeholder_text("Filtrar paquetes...");
    m_search_entry.set_hexpand(true);
    m_search_entry.signal_search_changed().connect(
        sigc::mem_fun(*this, &UpdatesTab::on_search_changed));

    m_filter_combo.append("Todos los paquetes");
    m_filter_combo.append("Solo con actualizaciones");
    m_filter_combo.set_active(0);
    m_filter_combo.signal_changed().connect(
        sigc::mem_fun(*this, &UpdatesTab::on_filter_changed));

    m_filter_box.pack_start(m_search_entry,  Gtk::PACK_EXPAND_WIDGET);
    m_filter_box.pack_start(m_filter_combo,  Gtk::PACK_SHRINK);

    pack_start(m_filter_box, Gtk::PACK_SHRINK);

    // Separator
    auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    pack_start(*sep, Gtk::PACK_SHRINK);

    // ---- Tree model ----
    m_model        = Gtk::ListStore::create(m_columns);
    m_filter_model = Gtk::TreeModelFilter::create(m_model);
    m_filter_model->set_visible_func(
        sigc::mem_fun(*this, &UpdatesTab::filter_func));
    m_sort_model = Gtk::TreeModelSort::create(m_filter_model);

    // ---- Tree view ----
    m_tree.set_model(m_sort_model);
    m_tree.set_rules_hint(true);

    {
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Nombre"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_name);
        col->set_sort_column(m_columns.col_name);
        col->set_resizable(true);
        col->set_min_width(150);
        m_tree.append_column(*col);
    }
    {
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Descripción"));
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
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Espacio"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_size);
        col->set_sort_column(m_columns.col_size);
        col->set_resizable(true);
        m_tree.append_column(*col);
    }
    {
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Versión actual"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_current_ver);
        col->set_resizable(true);
        m_tree.append_column(*col);
    }
    {
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Última versión"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        cell->property_foreground() = "#2d7d46";
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_latest_ver);
        col->set_resizable(true);
        m_tree.append_column(*col);
    }
    {
        auto* col = Gtk::manage(new Gtk::TreeViewColumn("Gestor"));
        auto* cell = Gtk::manage(new Gtk::CellRendererText());
        col->pack_start(*cell, true);
        col->add_attribute(*cell, "text", m_columns.col_manager);
        col->set_resizable(true);
        m_tree.append_column(*col);
    }

    // Action buttons column (rendered as two buttons per row using CellRendererPixbuf + tooltip)
    // We use a box in the last column to show "Update" and "Remove" buttons
    {
        auto* col   = Gtk::manage(new Gtk::TreeViewColumn("Acciones"));
        auto* btn_update = Gtk::manage(new Gtk::CellRendererPixbuf());
        auto* btn_remove = Gtk::manage(new Gtk::CellRendererPixbuf());
        btn_update->property_icon_name() = "system-software-update";
        btn_update->property_mode()      = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
        btn_update->property_sensitive()  = true;
        btn_remove->property_icon_name() = "edit-delete";
        btn_remove->property_mode()      = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
        btn_remove->property_sensitive()  = true;
        col->pack_start(*btn_update, false);
        col->pack_start(*btn_remove, false);
        col->set_resizable(false);
        m_tree.append_column(*col);

        m_tree.signal_button_press_event().connect([this, col](GdkEventButton* ev) -> bool {
            if (ev->type != GDK_BUTTON_PRESS || ev->button != 1)
                return false;
            Gtk::TreePath path;
            Gtk::TreeViewColumn* clicked_col = nullptr;
            int cell_x = 0, cell_y = 0;
            if (!m_tree.get_path_at_pos((int)ev->x, (int)ev->y,
                                        path, clicked_col, cell_x, cell_y))
                return false;
            if (clicked_col != col) return false;

            auto iter = m_sort_model->get_iter(path);
            if (!iter) return false;
            auto child = m_sort_model->convert_iter_to_child_iter(iter);
            auto child2 = m_filter_model->convert_iter_to_child_iter(child);
            Glib::ustring name    = (*child2)[m_columns.col_name];
            Glib::ustring manager = (*child2)[m_columns.col_manager];
            bool has_update       = (*child2)[m_columns.col_has_update];

            // Determine which icon was clicked (update or remove)
            // The first icon occupies roughly the first half, second the rest
            // We detect by cell width
            int total_width = 0;
            {
                Gdk::Rectangle rect;
                m_tree.get_cell_area(path, *col, rect);
                total_width = rect.get_width();
            }
            if (cell_x < total_width / 2) {
                if (has_update)
                    on_update_clicked(name, manager);
            } else {
                on_remove_clicked(name, manager);
            }
            return true;
        }, false);
    }

    m_scrolled.add(m_tree);
    m_scrolled.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    pack_start(m_scrolled, Gtk::PACK_EXPAND_WIDGET);

    show_all_children();
}

// ---------------------------------------------------------------------------

void UpdatesTab::populate(const std::vector<Package>& pkgs) {
    m_packages = pkgs;
    m_model->clear();
    for (auto& p : pkgs) {
        auto row = *m_model->append();
        row[m_columns.col_name]        = p.name;
        row[m_columns.col_desc]        = p.description;
        row[m_columns.col_size]        = p.installed_size;
        row[m_columns.col_current_ver] = p.current_version;
        row[m_columns.col_latest_ver]  = p.latest_version;
        row[m_columns.col_manager]     = p.manager;
        row[m_columns.col_has_update]  = p.has_update;
    }
    m_filter_model->refilter();
}

void UpdatesTab::refresh() {
    on_refresh_clicked();
}

void UpdatesTab::on_refresh_clicked() {
    set_busy(true);
    m_status_label.set_text("Cargando paquetes instalados...");

    std::thread([this]() {
        auto installed = m_pm.list_installed();
        auto updates   = m_pm.check_updates();

        // Merge update info into installed list
        for (auto& pkg : installed) {
            for (auto& upd : updates) {
                if (upd.name == pkg.name && upd.manager == pkg.manager) {
                    pkg.latest_version = upd.latest_version;
                    pkg.has_update     = true;
                    break;
                }
            }
        }

        Glib::signal_idle().connect_once([this, installed = std::move(installed),
                                          count = (int)updates.size()]() mutable {
            populate(installed);
            set_busy(false);
            m_status_label.set_text("Paquetes cargados. Actualizaciones disponibles: " +
                                    std::to_string(count));
        });
    }).detach();
}

void UpdatesTab::on_update_all_clicked() {
    std::string password;
    if (!ask_password("Actualizar todos los paquetes del sistema", password))
        return;

    set_busy(true);
    m_status_label.set_text("Actualizando todos los paquetes...");

    std::thread([this, password]() mutable {
        bool ok = m_pm.update_all(password, [this](const std::string& msg) {
            Glib::signal_idle().connect_once([this, msg]() {
                m_status_label.set_text(msg.substr(0, 120));
            });
        });
        Glib::signal_idle().connect_once([this, ok]() {
            set_busy(false);
            m_status_label.set_text(ok ? "Actualización completada." : "Error durante la actualización.");
            on_refresh_clicked();
        });
    }).detach();
}

void UpdatesTab::on_search_changed() {
    m_filter_model->refilter();
}

void UpdatesTab::on_filter_changed() {
    m_show_only_updates = (m_filter_combo.get_active_row_number() == 1);
    m_filter_model->refilter();
}

bool UpdatesTab::filter_func(const Gtk::TreeModel::const_iterator& iter) {
    if (!iter) return true;
    auto row = *iter;

    Glib::ustring name = row[m_columns.col_name];
    Glib::ustring desc = row[m_columns.col_desc];
    bool has_update    = row[m_columns.col_has_update];

    if (m_show_only_updates && !has_update)
        return false;

    Glib::ustring query = m_search_entry.get_text().lowercase();
    if (query.empty()) return true;

    return name.lowercase().find(query) != Glib::ustring::npos ||
           desc.lowercase().find(query) != Glib::ustring::npos;
}

bool UpdatesTab::ask_password(const std::string& reason, std::string& out_password) {
    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    if (!top) return false;

    AuthDialog dlg(*top, reason);
    if (dlg.run() != Gtk::RESPONSE_OK)
        return false;
    out_password = dlg.get_password();
    return true;
}

void UpdatesTab::on_update_clicked(const Glib::ustring& name, const Glib::ustring& /*manager*/) {
    std::string password;
    if (!ask_password("Actualizar paquete: " + name, password))
        return;

    set_busy(true);
    m_status_label.set_text("Actualizando " + name + "...");

    std::thread([this, name = name.raw(), password]() mutable {
        bool ok = m_pm.update_package(name, password);
        Glib::signal_idle().connect_once([this, ok, name]() {
            set_busy(false);
            m_status_label.set_text(ok ? (name + " actualizado correctamente.")
                                       : ("Error al actualizar " + name + "."));
            on_refresh_clicked();
        });
    }).detach();
}

void UpdatesTab::on_remove_clicked(const Glib::ustring& name, const Glib::ustring& /*manager*/) {
    // Confirm removal
    Gtk::Window* top = dynamic_cast<Gtk::Window*>(get_toplevel());
    Gtk::MessageDialog confirm(top ? *top : *(Gtk::Window*)nullptr,
                               "¿Eliminar el paquete «" + name + "»?",
                               false, Gtk::MESSAGE_QUESTION,
                               Gtk::BUTTONS_YES_NO, true);
    confirm.set_secondary_text("Esta acción desinstalará el paquete del sistema.");
    if (confirm.run() != Gtk::RESPONSE_YES) return;

    std::string password;
    if (!ask_password("Eliminar paquete: " + name, password))
        return;

    set_busy(true);
    m_status_label.set_text("Eliminando " + name + "...");

    std::thread([this, name = name.raw(), password]() mutable {
        bool ok = m_pm.remove_package(name, password);
        Glib::signal_idle().connect_once([this, ok, name]() {
            set_busy(false);
            m_status_label.set_text(ok ? (name + " eliminado correctamente.")
                                       : ("Error al eliminar " + name + "."));
            on_refresh_clicked();
        });
    }).detach();
}

void UpdatesTab::set_busy(bool busy) {
    if (busy) {
        m_spinner.start();
        m_refresh_btn.set_sensitive(false);
        m_update_all_btn.set_sensitive(false);
    } else {
        m_spinner.stop();
        m_refresh_btn.set_sensitive(true);
        m_update_all_btn.set_sensitive(true);
    }
}
