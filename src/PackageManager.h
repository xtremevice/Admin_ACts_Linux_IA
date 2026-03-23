#pragma once

#include <string>
#include <vector>
#include <functional>

// Represents an installed or available package
struct Package {
    std::string name;
    std::string description;
    std::string current_version;   // empty if not installed
    std::string latest_version;
    std::string installed_size;    // human-readable, e.g. "4.5 MB"
    std::string manager;           // "apt" or "pacman"
    bool has_update = false;
};

// Represents a software repository
struct Repository {
    std::string file_path;         // e.g. /etc/apt/sources.list.d/ubuntu.list
    std::string line;              // the full repository line
    std::string type;              // "deb", "deb-src", etc.
    std::string uri;
    std::string suite;
    std::string components;
    bool enabled = true;           // false = commented out
    std::string manager;           // "apt" or "pacman"
};

// Callback types for async operations
using ProgressCallback = std::function<void(const std::string& message)>;
using DoneCallback     = std::function<void(bool success, const std::string& output)>;

class PackageManager {
public:
    PackageManager();

    // ---- Installed packages & updates ----
    std::vector<Package> list_installed();
    std::vector<Package> check_updates();

    // ---- Search ----
    std::vector<Package> search(const std::string& query);

    // ---- Package operations (need root password) ----
    bool install_package(const std::string& name,
                         const std::string& root_password,
                         ProgressCallback progress = nullptr);

    bool remove_package(const std::string& name,
                        const std::string& root_password,
                        ProgressCallback progress = nullptr);

    bool update_package(const std::string& name,
                        const std::string& root_password,
                        ProgressCallback progress = nullptr);

    bool update_all(const std::string& root_password,
                    ProgressCallback progress = nullptr);

    // ---- Repositories ----
    std::vector<Repository> list_repositories();
    bool enable_repository(const Repository& repo,
                           const std::string& root_password);
    bool disable_repository(const Repository& repo,
                            const std::string& root_password);
    bool add_repository(const std::string& line,
                        const std::string& file_path,
                        const std::string& root_password);

    // ---- Autostart ----
    bool get_autostart() const;
    bool set_autostart(bool enable, const std::string& root_password);

    // ---- Package managers available ----
    bool has_apt()    const { return m_has_apt; }
    bool has_pacman() const { return m_has_pacman; }

private:
    bool m_has_apt    = false;
    bool m_has_pacman = false;

    // Run a command with sudo, passing root_password via stdin.
    // Returns {exit_code, combined_stdout_stderr}
    std::pair<int, std::string> run_sudo(const std::string& command,
                                         const std::string& root_password);

    // Run a command without elevation, return {exit_code, stdout}
    std::pair<int, std::string> run_command(const std::string& command);

    // Parse apt installed packages
    std::vector<Package> list_apt_installed();
    std::vector<Package> list_pacman_installed();

    // Parse apt upgradable packages
    std::vector<Package> apt_upgradable();
    std::vector<Package> pacman_upgradable();

    std::vector<Repository> list_apt_repos();
    std::vector<Repository> list_pacman_repos();
};
