# Admin Acts Linux

Administrador de paquetes con interfaz gráfica (GTK3 / C++17) para Linux.  
Permite actualizar, instalar y gestionar paquetes de APT y Pacman desde una
ventana unificada con tres pestañas: **Actualizaciones**, **Instalar** y
**Configuración**.

---

## Distribución y empaquetado

El script `build.sh` de la raíz del repositorio compila el proyecto y genera
los paquetes distribuibles con **un solo comando**.  
Se producen dos formatos para cubrir todas las distribuciones populares:

| Formato | Distros objetivo | Ventajas |
|---------|------------------|----------|
| `.deb`  | Debian, Ubuntu, Linux Mint, Pop!_OS, … | integración nativa con APT, se actualiza con `apt upgrade` |
| `AppImage` | Fedora, Arch, openSUSE, Manjaro, y cualquier otra distro | sin instalación, portable, autocontenido |

### Dependencias de compilación

```bash
# Debian / Ubuntu / Mint
sudo apt install cmake make g++ pkg-config libgtkmm-3.0-dev fakeroot patchelf

# Fedora
sudo dnf install cmake make gcc-c++ pkgconf gtkmm30-devel patchelf

# Arch / Manjaro
sudo pacman -S cmake make gcc pkgconf gtkmm3 patchelf
```

### Generar ambos paquetes (predeterminado)

```bash
bash build.sh
```

Los archivos se guardan en `dist/`:

```
dist/
  admin-acts-linux_1.0.0_amd64.deb
  Admin_Acts_Linux-x86_64.AppImage
```

### Opciones del script

```bash
bash build.sh --deb        # Solo el .deb
bash build.sh --appimage   # Solo el AppImage
bash build.sh --all        # Ambos (equivalente a no pasar argumentos)
bash build.sh --help       # Muestra la ayuda
```

### Instalar el .deb

```bash
sudo dpkg -i dist/admin-acts-linux_1.0.0_amd64.deb
# Instalar dependencias faltantes si las hubiera:
sudo apt install -f
```

### Ejecutar el AppImage (sin instalación)

```bash
chmod +x dist/Admin_Acts_Linux-x86_64.AppImage
./dist/Admin_Acts_Linux-x86_64.AppImage
```

---

## Compilar sin empaquetar

```bash
cmake -B _build -DCMAKE_BUILD_TYPE=Release
cmake --build _build --parallel
./_build/admin-acts-linux
```
