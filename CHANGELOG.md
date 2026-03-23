# CHANGELOG – Admin Acts Linux

Este archivo registra todos los cambios, solicitudes y peticiones del proyecto
para facilitar el seguimiento del desarrollo.

---

## [1.0.0] – 2026-03-23

### Funcionalidades implementadas

#### Pestaña 1: Actualizaciones (`UpdatesTab`)
- Lista completa de todos los paquetes instalados en el sistema.
- Columnas: Nombre, Descripción, Espacio usado, Versión actual, Última versión, Gestor, Acciones.
- Soporte para APT (Debian/Ubuntu) y Pacman (Arch Linux) simultáneamente.
- Botón **Actualizar lista** con spinner de carga mientras trabaja en segundo plano.
- Botón **Actualizar todo** que ejecuta `apt-get dist-upgrade` o `pacman -Syu` con contraseña sudo.
- Filtro en tiempo real por nombre/descripción.
- Combo para mostrar solo paquetes con actualizaciones disponibles.
- Botón **Actualizar** por paquete individual (solo visible cuando hay actualización disponible).
- Botón **Eliminar** por paquete con confirmación y autenticación.
- Todas las operaciones de red/disco se realizan en hilos secundarios para no bloquear la UI.

#### Pestaña 2: Instalar (`InstallTab`)
- Campo de búsqueda con botón **Buscar** (también activable con Enter).
- Búsqueda simultánea en APT (`apt-cache search`) y Pacman (`pacman -Ss`).
- Resultados: Nombre, Última versión, Descripción/Funciones, Gestor de paquetes.
- Botón **Instalar** por resultado; si ya está instalado muestra icono de verificado y desactiva el botón.
- Al instalar se resuelven y descargan **todas las dependencias** transitivas automáticamente
  (`apt-get install -y` / `pacman -S --noconfirm`).
- Progreso en tiempo real en la barra de estado.

#### Pestaña 3: Configuración (`SettingsTab`)
- **Autostart**: checkbox para activar/desactivar inicio automático del sistema
  (crea/elimina `~/.config/autostart/admin-acts-linux.desktop`).
- **Horario de actualizaciones**: rango horario (hora desde / hora hasta) para
  actualizaciones automáticas, persistido en `~/.config/admin-acts-linux/settings.conf`.
- **Repositorios**: lista agrupada por archivo de todos los repositorios:
  - `/etc/apt/sources.list`
  - `/etc/apt/sources.list.d/*.list`
  - `/etc/pacman.conf` (si pacman está disponible)
  - Columnas: Activo (toggle), Gestor, Tipo, URI, Suite, Componentes.
  - Toggle para activar (descomentar la línea) o desactivar (comentar la línea).
  - Botón **Agregar repositorio** con diálogo para ingresar la línea y el archivo destino.
- **Autenticación**: cualquier operación que requiere privilegios de root lanza
  `AuthDialog` solicitando la contraseña, que se pasa a `sudo -S` de forma segura.

#### Componentes transversales
- **PackageManager**: backend en C++ puro que encapsula todas las operaciones
  de paquetes y repositorios usando `apt`, `apt-get`, `dpkg-query`, `pacman`.
- **AuthDialog**: diálogo GTK reutilizable para solicitar contraseña de administrador
  antes de cualquier operación privilegiada.
- Interfaz completamente en **español**.
- Construido con **C++17** y **gtkmm-3.0** para máxima eficiencia y nativas capacidades GTK.
- Sistema de compilación: **CMake**.

---

## Solicitudes pendientes / Mejoras futuras

- [ ] Soporte para Flatpak y Snap como gestores adicionales.
- [ ] Notificaciones de escritorio (libnotify) cuando hay actualizaciones disponibles.
- [ ] Vista de detalles expandida por paquete (changelog, dependencias, archivos instalados).
- [ ] Integración con systemd timer para actualizaciones automáticas según el horario configurado.
- [ ] Caché local de resultados de búsqueda para respuestas más rápidas.
- [ ] Botón "Limpiar caché APT" (`apt-get clean`).
- [ ] Historial de operaciones (log de instalaciones/eliminaciones).
- [ ] Soporte para importar claves GPG de repositorios nuevos.
- [ ] Modo oscuro / selección de tema.
- [ ] Empaquetado como .deb y .pkg.tar.zst para distribución.

---

## Notas técnicas

- Las operaciones de paquetes se ejecutan en hilos secundarios (`std::thread`)
  para no bloquear el hilo principal de GTK.
- Los resultados se devuelven al hilo principal usando `Glib::signal_idle().connect_once()`.
- La contraseña sudo se pasa por stdin (`echo pass | sudo -S`) con las comillas
  simples escapadas para prevenir inyección de shell.
- El autostart usa el estándar XDG (`.config/autostart/`) sin necesidad de root.
- La configuración del horario se persiste en `~/.config/admin-acts-linux/settings.conf`.
