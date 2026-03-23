#pragma once

#include <gtkmm.h>
#include <string>

// Dialog that prompts the user for the root/sudo password.
class AuthDialog : public Gtk::Dialog {
public:
    explicit AuthDialog(Gtk::Window& parent, const std::string& reason = "");
    ~AuthDialog() override = default;

    std::string get_password() const;

private:
    Gtk::Box       m_vbox;
    Gtk::Label     m_reason_label;
    Gtk::Label     m_pw_label;
    Gtk::Entry     m_pw_entry;
    Gtk::Box       m_pw_box;
};
