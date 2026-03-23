#include "AuthDialog.h"

AuthDialog::AuthDialog(Gtk::Window& parent, const std::string& reason)
    : Gtk::Dialog("Autenticación requerida", parent, true),
      m_vbox(Gtk::ORIENTATION_VERTICAL, 8),
      m_pw_box(Gtk::ORIENTATION_HORIZONTAL, 6)
{
    set_default_size(380, -1);
    set_border_width(12);

    auto* content = get_content_area();
    content->set_spacing(8);

    // Icon + reason
    auto* hbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
    auto* icon = Gtk::manage(new Gtk::Image());
    icon->set_from_icon_name("dialog-password", Gtk::ICON_SIZE_DIALOG);
    hbox->pack_start(*icon, Gtk::PACK_SHRINK);

    auto* info_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
    auto* title_lbl = Gtk::manage(new Gtk::Label());
    title_lbl->set_markup("<b>Se requiere contraseña de administrador</b>");
    title_lbl->set_halign(Gtk::ALIGN_START);
    info_box->pack_start(*title_lbl, Gtk::PACK_SHRINK);

    if (!reason.empty()) {
        auto* reason_lbl = Gtk::manage(new Gtk::Label(reason));
        reason_lbl->set_halign(Gtk::ALIGN_START);
        reason_lbl->set_line_wrap(true);
        reason_lbl->set_max_width_chars(45);
        info_box->pack_start(*reason_lbl, Gtk::PACK_SHRINK);
    }

    hbox->pack_start(*info_box, Gtk::PACK_EXPAND_WIDGET);
    content->pack_start(*hbox, Gtk::PACK_SHRINK);

    // Separator
    auto* sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    content->pack_start(*sep, Gtk::PACK_SHRINK);

    // Password row
    auto* pw_row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
    auto* pw_lbl = Gtk::manage(new Gtk::Label("Contraseña:"));
    pw_lbl->set_halign(Gtk::ALIGN_END);
    pw_lbl->set_size_request(110, -1);

    m_pw_entry.set_visibility(false);
    m_pw_entry.set_activates_default(true);
    m_pw_entry.set_hexpand(true);

    pw_row->pack_start(*pw_lbl,    Gtk::PACK_SHRINK);
    pw_row->pack_start(m_pw_entry, Gtk::PACK_EXPAND_WIDGET);
    content->pack_start(*pw_row, Gtk::PACK_SHRINK);

    // Buttons
    add_button("Cancelar", Gtk::RESPONSE_CANCEL);
    auto* ok_btn = add_button("Autenticar", Gtk::RESPONSE_OK);
    ok_btn->get_style_context()->add_class("suggested-action");
    set_default_response(Gtk::RESPONSE_OK);

    show_all_children();
}

std::string AuthDialog::get_password() const {
    return m_pw_entry.get_text();
}
