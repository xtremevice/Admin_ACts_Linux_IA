#include "PackageManager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty())
            out.push_back(line);
    }
    return out;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool file_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}

// ---------------------------------------------------------------------------

PackageManager::PackageManager() {
    m_has_apt    = file_exists("/usr/bin/apt") || file_exists("/usr/bin/apt-get");
    m_has_pacman = file_exists("/usr/bin/pacman");
}

// ---------------------------------------------------------------------------
// run_command – execute without elevation
// ---------------------------------------------------------------------------
std::pair<int, std::string> PackageManager::run_command(const std::string& command) {
    std::string full = command + " 2>&1";
    FILE* fp = popen(full.c_str(), "r");
    if (!fp) return {-1, "popen failed"};

    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp))
        output += buf;

    int ret = pclose(fp);
    return {WEXITSTATUS(ret), output};
}

// ---------------------------------------------------------------------------
// run_sudo – run command with sudo, password via stdin
// ---------------------------------------------------------------------------
std::pair<int, std::string> PackageManager::run_sudo(const std::string& command,
                                                      const std::string& root_password) {
    // Use sudo -S to read password from stdin
    // We wrap with bash -c to combine password echo with the command
    std::string escaped_pass = root_password;
    // Escape single quotes in the password
    size_t pos = 0;
    while ((pos = escaped_pass.find('\'', pos)) != std::string::npos) {
        escaped_pass.replace(pos, 1, "'\\''");
        pos += 4;
    }

    std::string full = "echo '" + escaped_pass + "' | sudo -S " + command + " 2>&1";
    FILE* fp = popen(full.c_str(), "r");
    if (!fp) return {-1, "popen failed"};

    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp))
        output += buf;

    int ret = pclose(fp);
    return {WEXITSTATUS(ret), output};
}

// ---------------------------------------------------------------------------
// list_apt_installed
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::list_apt_installed() {
    std::vector<Package> packages;
    if (!m_has_apt) return packages;

    // dpkg-query: name, version, installed-size, description
    auto [rc, out] = run_command(
        "dpkg-query -W -f='${Package}\\t${Version}\\t${Installed-Size}\\t${binary:Summary}\\n'");
    if (rc != 0) return packages;

    for (auto& line : split_lines(out)) {
        std::istringstream ss(line);
        std::string name, version, size_kb, desc;
        std::getline(ss, name,    '\t');
        std::getline(ss, version, '\t');
        std::getline(ss, size_kb, '\t');
        std::getline(ss, desc);

        if (name.empty()) continue;

        Package p;
        p.name    = name;
        p.current_version = version;
        p.latest_version  = version;  // will be updated by check_updates()
        p.description = desc;
        p.manager   = "apt";

        // Convert KB to human-readable
        try {
            long kb = std::stol(size_kb);
            if (kb >= 1024 * 1024)
                p.installed_size = std::to_string(kb / (1024*1024)) + " GB";
            else if (kb >= 1024)
                p.installed_size = std::to_string(kb / 1024) + " MB";
            else
                p.installed_size = size_kb + " KB";
        } catch (...) {
            p.installed_size = size_kb + " KB";
        }

        packages.push_back(std::move(p));
    }
    return packages;
}

// ---------------------------------------------------------------------------
// list_pacman_installed
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::list_pacman_installed() {
    std::vector<Package> packages;
    if (!m_has_pacman) return packages;

    auto [rc, out] = run_command("pacman -Q 2>/dev/null");
    if (rc != 0) return packages;

    for (auto& line : split_lines(out)) {
        std::istringstream ss(line);
        std::string name, version;
        ss >> name >> version;
        if (name.empty()) continue;

        Package p;
        p.name    = name;
        p.current_version = version;
        p.latest_version  = version;
        p.manager = "pacman";
        p.installed_size = "N/A";

        // Get description separately
        auto [rc2, desc_out] = run_command("pacman -Qi " + name +
                                           " 2>/dev/null | grep '^Description' | head -1");
        if (!desc_out.empty()) {
            auto colon = desc_out.find(':');
            if (colon != std::string::npos)
                p.description = trim(desc_out.substr(colon + 1));
        }
        packages.push_back(std::move(p));
    }
    return packages;
}

// ---------------------------------------------------------------------------
// list_installed
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::list_installed() {
    auto packages = list_apt_installed();
    auto pacman_pkg = list_pacman_installed();
    packages.insert(packages.end(), pacman_pkg.begin(), pacman_pkg.end());
    return packages;
}

// ---------------------------------------------------------------------------
// apt_upgradable
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::apt_upgradable() {
    std::vector<Package> packages;
    if (!m_has_apt) return packages;

    // First update the package lists (no root needed for reading)
    auto [rc, out] = run_command("apt list --upgradable 2>/dev/null");
    if (rc != 0) return packages;

    // Format: name/suite version arch [upgradable from: old_version]
    static const std::regex re(R"(^([^/]+)/\S+\s+(\S+)\s+\S+\s+\[upgradable from: (\S+)\])");
    for (auto& line : split_lines(out)) {
        std::smatch m;
        if (!std::regex_search(line, m, re)) continue;

        Package p;
        p.name            = m[1].str();
        p.latest_version  = m[2].str();
        p.current_version = m[3].str();
        p.manager         = "apt";
        p.has_update      = true;

        // Get description and size
        auto [rc2, info] = run_command("apt-cache show " + p.name + " 2>/dev/null | head -20");
        for (auto& iline : split_lines(info)) {
            if (iline.rfind("Description-en:", 0) == 0 || iline.rfind("Description:", 0) == 0) {
                auto colon = iline.find(':');
                p.description = trim(iline.substr(colon + 1));
            }
            if (iline.rfind("Installed-Size:", 0) == 0) {
                auto colon = iline.find(':');
                std::string sz = trim(iline.substr(colon + 1));
                try {
                    long kb = std::stol(sz);
                    if (kb >= 1024*1024)
                        p.installed_size = std::to_string(kb/(1024*1024)) + " GB";
                    else if (kb >= 1024)
                        p.installed_size = std::to_string(kb/1024) + " MB";
                    else
                        p.installed_size = sz + " KB";
                } catch (...) {
                    p.installed_size = sz + " KB";
                }
            }
        }
        packages.push_back(std::move(p));
    }
    return packages;
}

// ---------------------------------------------------------------------------
// pacman_upgradable
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::pacman_upgradable() {
    std::vector<Package> packages;
    if (!m_has_pacman) return packages;

    auto [rc, out] = run_command("pacman -Qu 2>/dev/null");
    if (rc != 0) return packages;

    for (auto& line : split_lines(out)) {
        std::istringstream ss(line);
        std::string name, cur, arrow, latest;
        ss >> name >> cur >> arrow >> latest;
        if (name.empty()) continue;

        Package p;
        p.name            = name;
        p.current_version = cur;
        p.latest_version  = latest;
        p.manager         = "pacman";
        p.has_update      = true;
        packages.push_back(std::move(p));
    }
    return packages;
}

// ---------------------------------------------------------------------------
// check_updates
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::check_updates() {
    auto packages = apt_upgradable();
    auto pacman_pkg = pacman_upgradable();
    packages.insert(packages.end(), pacman_pkg.begin(), pacman_pkg.end());
    return packages;
}

// ---------------------------------------------------------------------------
// search
// ---------------------------------------------------------------------------
std::vector<Package> PackageManager::search(const std::string& query) {
    std::vector<Package> packages;

    if (m_has_apt) {
        auto [rc, out] = run_command("apt-cache search " + query + " 2>/dev/null");
        if (rc == 0) {
            for (auto& line : split_lines(out)) {
                auto dash = line.find(" - ");
                if (dash == std::string::npos) continue;

                Package p;
                p.name        = trim(line.substr(0, dash));
                p.description = trim(line.substr(dash + 3));
                p.manager     = "apt";

                // Get version info
                auto [rc2, ver_out] = run_command(
                    "apt-cache policy " + p.name + " 2>/dev/null | head -5");
                for (auto& vl : split_lines(ver_out)) {
                    if (vl.find("Installed:") != std::string::npos) {
                        auto colon = vl.find(':');
                        p.current_version = trim(vl.substr(colon + 1));
                        if (p.current_version == "(none)")
                            p.current_version.clear();
                    }
                    if (vl.find("Candidate:") != std::string::npos) {
                        auto colon = vl.find(':');
                        p.latest_version = trim(vl.substr(colon + 1));
                    }
                }
                packages.push_back(std::move(p));
            }
        }
    }

    if (m_has_pacman) {
        auto [rc, out] = run_command("pacman -Ss " + query + " 2>/dev/null");
        if (rc == 0) {
            auto lines = split_lines(out);
            for (size_t i = 0; i + 1 < lines.size(); i += 2) {
                // line[i]:   repo/name version [installed]
                // line[i+1]: description
                std::istringstream ss(lines[i]);
                std::string repo_name, version;
                ss >> repo_name >> version;

                auto slash = repo_name.find('/');
                if (slash == std::string::npos) continue;

                Package p;
                p.name           = repo_name.substr(slash + 1);
                p.latest_version = version;
                p.description    = trim(lines[i + 1]);
                p.manager        = "pacman";
                if (lines[i].find("[installed]") != std::string::npos)
                    p.current_version = version;
                packages.push_back(std::move(p));
            }
        }
    }

    return packages;
}

// ---------------------------------------------------------------------------
// install_package
// ---------------------------------------------------------------------------
bool PackageManager::install_package(const std::string& name,
                                     const std::string& root_password,
                                     ProgressCallback progress) {
    std::string cmd;
    if (m_has_apt)
        cmd = "DEBIAN_FRONTEND=noninteractive apt-get install -y " + name;
    else if (m_has_pacman)
        cmd = "pacman -S --noconfirm " + name;
    else
        return false;

    if (progress) progress("Installing " + name + "...");
    auto [rc, out] = run_sudo(cmd, root_password);
    if (progress) progress(out);
    return rc == 0;
}

// ---------------------------------------------------------------------------
// remove_package
// ---------------------------------------------------------------------------
bool PackageManager::remove_package(const std::string& name,
                                    const std::string& root_password,
                                    ProgressCallback progress) {
    std::string cmd;
    if (m_has_apt)
        cmd = "DEBIAN_FRONTEND=noninteractive apt-get remove -y " + name;
    else if (m_has_pacman)
        cmd = "pacman -R --noconfirm " + name;
    else
        return false;

    if (progress) progress("Removing " + name + "...");
    auto [rc, out] = run_sudo(cmd, root_password);
    if (progress) progress(out);
    return rc == 0;
}

// ---------------------------------------------------------------------------
// update_package
// ---------------------------------------------------------------------------
bool PackageManager::update_package(const std::string& name,
                                    const std::string& root_password,
                                    ProgressCallback progress) {
    std::string cmd;
    if (m_has_apt)
        cmd = "DEBIAN_FRONTEND=noninteractive apt-get install --only-upgrade -y " + name;
    else if (m_has_pacman)
        cmd = "pacman -S --noconfirm " + name;
    else
        return false;

    if (progress) progress("Updating " + name + "...");
    auto [rc, out] = run_sudo(cmd, root_password);
    if (progress) progress(out);
    return rc == 0;
}

// ---------------------------------------------------------------------------
// update_all
// ---------------------------------------------------------------------------
bool PackageManager::update_all(const std::string& root_password,
                                ProgressCallback progress) {
    bool ok = true;
    if (m_has_apt) {
        if (progress) progress("Updating APT package lists...");
        {
            auto [rc, out] = run_sudo("apt-get update", root_password);
            if (rc != 0) ok = false;
            if (progress) progress(out);
        }
        if (progress) progress("Upgrading all packages...");
        {
            auto [rc, out] = run_sudo(
                "DEBIAN_FRONTEND=noninteractive apt-get dist-upgrade -y", root_password);
            if (rc != 0) ok = false;
            if (progress) progress(out);
        }
    }
    if (m_has_pacman) {
        if (progress) progress("Upgrading all pacman packages...");
        auto [rc, out] = run_sudo("pacman -Syu --noconfirm", root_password);
        if (rc != 0) ok = false;
        if (progress) progress(out);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// list_apt_repos
// ---------------------------------------------------------------------------
std::vector<Repository> PackageManager::list_apt_repos() {
    std::vector<Repository> repos;

    // Collect source files: /etc/apt/sources.list + /etc/apt/sources.list.d/*.list
    std::vector<std::string> files;
    if (file_exists("/etc/apt/sources.list"))
        files.push_back("/etc/apt/sources.list");

    std::error_code ec;
    for (auto& entry : fs::directory_iterator("/etc/apt/sources.list.d", ec)) {
        auto path = entry.path().string();
        auto ends_with = [](const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() &&
                   s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        };
        if (ends_with(path, ".list") || ends_with(path, ".sources"))
            files.push_back(path);
    }

    for (auto& fpath : files) {
        std::ifstream ifs(fpath);
        if (!ifs) continue;

        std::string line;
        while (std::getline(ifs, line)) {
            if (trim(line).empty()) continue;

            Repository r;
            r.file_path = fpath;
            r.manager   = "apt";
            r.line      = line;

            // Detect if commented out
            std::string content = trim(line);
            if (content[0] == '#') {
                r.enabled = false;
                content   = trim(content.substr(1));
            } else {
                r.enabled = true;
            }

            // Parse: type uri suite components
            std::istringstream ss(content);
            ss >> r.type >> r.uri >> r.suite;
            std::string comp;
            while (ss >> comp)
                r.components += comp + " ";
            r.components = trim(r.components);

            if (!r.type.empty() && (r.type == "deb" || r.type == "deb-src"))
                repos.push_back(std::move(r));
        }
    }
    return repos;
}

// ---------------------------------------------------------------------------
// list_pacman_repos
// ---------------------------------------------------------------------------
std::vector<Repository> PackageManager::list_pacman_repos() {
    std::vector<Repository> repos;
    if (!m_has_pacman) return repos;

    std::ifstream ifs("/etc/pacman.conf");
    if (!ifs) return repos;

    std::string section;
    std::string line;
    while (std::getline(ifs, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;

        if (t[0] == '[') {
            section = t.substr(1, t.find(']') - 1);
            continue;
        }
        if (section.empty() || section == "options") continue;

        if (t.rfind("Server", 0) == 0 || t.rfind("#Server", 0) == 0) {
            Repository r;
            r.file_path  = "/etc/pacman.conf";
            r.manager    = "pacman";
            r.line       = line;
            r.type       = section;  // repo name as type
            r.enabled    = (t[0] != '#');
            auto eq      = t.find('=');
            if (eq != std::string::npos)
                r.uri = trim(t.substr(eq + 1));
            repos.push_back(std::move(r));
        }
    }
    return repos;
}

// ---------------------------------------------------------------------------
// list_repositories
// ---------------------------------------------------------------------------
std::vector<Repository> PackageManager::list_repositories() {
    auto repos = list_apt_repos();
    auto pacman_repos = list_pacman_repos();
    repos.insert(repos.end(), pacman_repos.begin(), pacman_repos.end());
    return repos;
}

// ---------------------------------------------------------------------------
// enable_repository / disable_repository
// ---------------------------------------------------------------------------
bool PackageManager::enable_repository(const Repository& repo,
                                       const std::string& root_password) {
    if (repo.enabled) return true;

    // Uncomment the line: remove leading '#'
    std::string orig = repo.line;
    std::string uncommented = trim(orig);
    if (!uncommented.empty() && uncommented[0] == '#')
        uncommented = uncommented.substr(1);
    if (!uncommented.empty() && uncommented[0] == ' ')
        uncommented = uncommented.substr(1);

    // Use sed to replace the exact line in the file
    std::string esc_orig = orig;
    // Escape / and & for sed
    auto esc = [](std::string s) {
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '/' || s[i] == '&' || s[i] == '\\' || s[i] == '\'' || s[i] == '\n') {
                s.insert(i, "\\");
                i++;
            }
        }
        return s;
    };

    std::string cmd = "sed -i 's|^" + esc(orig) + "|" + esc(uncommented) + "|' " + repo.file_path;
    auto [rc, out] = run_sudo(cmd, root_password);
    return rc == 0;
}

bool PackageManager::disable_repository(const Repository& repo,
                                        const std::string& root_password) {
    if (!repo.enabled) return true;

    std::string commented = "# " + repo.line;

    auto esc = [](std::string s) {
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '/' || s[i] == '&' || s[i] == '\\' || s[i] == '\'') {
                s.insert(i, "\\");
                i++;
            }
        }
        return s;
    };

    std::string cmd = "sed -i 's|^" + esc(repo.line) + "|" + esc(commented) + "|' " + repo.file_path;
    auto [rc, out] = run_sudo(cmd, root_password);
    return rc == 0;
}

// ---------------------------------------------------------------------------
// add_repository
// ---------------------------------------------------------------------------
bool PackageManager::add_repository(const std::string& line,
                                    const std::string& file_path,
                                    const std::string& root_password) {
    std::string cmd = "bash -c 'echo \"" + line + "\" >> " + file_path + "'";
    auto [rc, out] = run_sudo(cmd, root_password);
    return rc == 0;
}

// ---------------------------------------------------------------------------
// Autostart (via systemd user service or XDG autostart)
// ---------------------------------------------------------------------------
static const std::string AUTOSTART_DIR = std::string(getenv("HOME") ? getenv("HOME") : "/root") +
                                          "/.config/autostart";
static const std::string AUTOSTART_FILE = AUTOSTART_DIR + "/admin-acts-linux.desktop";
static const std::string DESKTOP_CONTENT =
    "[Desktop Entry]\n"
    "Type=Application\n"
    "Name=Admin Acts Linux\n"
    "Exec=admin-acts-linux\n"
    "Hidden=false\n"
    "NoDisplay=false\n"
    "X-GNOME-Autostart-enabled=true\n";

bool PackageManager::get_autostart() const {
    return file_exists(AUTOSTART_FILE);
}

bool PackageManager::set_autostart(bool enable, const std::string& /*root_password*/) {
    if (enable) {
        // Create autostart directory if needed
        fs::create_directories(AUTOSTART_DIR);
        std::ofstream ofs(AUTOSTART_FILE);
        if (!ofs) return false;
        ofs << DESKTOP_CONTENT;
        return true;
    } else {
        std::error_code ec;
        fs::remove(AUTOSTART_FILE, ec);
        return !ec;
    }
}
