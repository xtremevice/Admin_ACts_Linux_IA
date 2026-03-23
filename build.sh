#!/usr/bin/env bash
# =============================================================================
# build.sh – Genera los paquetes distribuibles de Admin Acts Linux.
#
# Uso:   bash build.sh [--deb] [--appimage] [--all]
#
#   --deb        Genera sólo el paquete .deb (Debian / Ubuntu / Mint)
#   --appimage   Genera sólo el AppImage (todas las distros)
#   --all        Genera ambos (predeterminado si no se pasa ningún argumento)
#
# Requisitos del sistema para --deb:
#   cmake  make  g++  pkg-config  libgtkmm-3.0-dev  fakeroot  dpkg-deb
#
# Requisitos del sistema para --appimage:
#   cmake  make  g++  pkg-config  libgtkmm-3.0-dev  patchelf
#   (appimagetool se descarga automáticamente si no se encuentra en PATH)
#
# Salida:  dist/
#   admin-acts-linux_1.0.0_amd64.deb
#   Admin_Acts_Linux-x86_64.AppImage
# =============================================================================

set -euo pipefail

# ─── Variables configurables ──────────────────────────────────────────────────
APPNAME="admin-acts-linux"
VERSION="1.0.0"
ARCH="$(uname -m)"                    # x86_64, aarch64, …
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${SCRIPT_DIR}/dist"
BUILD_DIR="${SCRIPT_DIR}/_build"
APPIMAGE_TOOL_URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"

# ─── Colores para la salida ───────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
log()  { echo -e "${GREEN}[build.sh]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
err()  { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# ─── Procesamiento de argumentos ─────────────────────────────────────────────
BUILD_DEB=false
BUILD_APPIMAGE=false

if [[ $# -eq 0 ]]; then
    BUILD_DEB=true
    BUILD_APPIMAGE=true
else
    for arg in "$@"; do
        case "$arg" in
            --deb)      BUILD_DEB=true ;;
            --appimage) BUILD_APPIMAGE=true ;;
            --all)      BUILD_DEB=true; BUILD_APPIMAGE=true ;;
            -h|--help)
                echo "Uso: bash build.sh [--deb] [--appimage] [--all]"
                exit 0 ;;
            *) err "Argumento desconocido: $arg  (usa --help)" ;;
        esac
    done
fi

# ─── Verificar dependencias de compilación ────────────────────────────────────
check_build_deps() {
    local missing=()
    for cmd in cmake make g++ pkg-config; do
        command -v "$cmd" > /dev/null 2>&1 || missing+=("$cmd")
    done
    if ! pkg-config --exists gtkmm-3.0 2>/dev/null; then
        missing+=("libgtkmm-3.0-dev")
    fi
    if [[ ${#missing[@]} -gt 0 ]]; then
        err "Faltan dependencias de compilación: ${missing[*]}\n  En Ubuntu/Debian instala con:\n  sudo apt install cmake make g++ pkg-config libgtkmm-3.0-dev fakeroot patchelf"
    fi
}

# ─── Compilar el binario ──────────────────────────────────────────────────────
compile() {
    log "Compilando con CMake en '${BUILD_DIR}' …"
    cmake -S "${SCRIPT_DIR}" \
          -B "${BUILD_DIR}" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
    log "Compilación completada."
}

# ─────────────────────────────────────────────────────────────────────────────
# CONSTRUCCIÓN DEL .DEB
# ─────────────────────────────────────────────────────────────────────────────
build_deb() {
    log "Construyendo paquete .deb …"

    # ── verificar herramientas adicionales ────────────────────────────────────
    for cmd in fakeroot dpkg-deb; do
        command -v "$cmd" > /dev/null 2>&1 || err "'$cmd' no encontrado. Instala con: sudo apt install fakeroot dpkg"
    done

    local DEB_STAGING="${BUILD_DIR}/deb-staging"
    local DEB_INSTALL="${DEB_STAGING}/usr"

    # Instalar en el directorio de staging
    DESTDIR="${DEB_STAGING}" cmake --install "${BUILD_DIR}"

    # Copiar ficheros debian
    local DEBIAN_DIR="${DEB_STAGING}/DEBIAN"
    mkdir -p "${DEBIAN_DIR}"
    cp "${SCRIPT_DIR}/packaging/debian/control"  "${DEBIAN_DIR}/control"
    cp "${SCRIPT_DIR}/packaging/debian/changelog" "${DEBIAN_DIR}/changelog"
    cp "${SCRIPT_DIR}/packaging/debian/postinst"  "${DEBIAN_DIR}/postinst"
    chmod 0755 "${DEBIAN_DIR}/postinst"

    # Corregir la arquitectura en el control si el sistema no es amd64
    if [[ "${ARCH}" != "x86_64" ]]; then
        local deb_arch
        deb_arch="$(dpkg --print-architecture 2>/dev/null || echo "${ARCH}")"
        sed -i "s/^Architecture: .*/Architecture: ${deb_arch}/" "${DEBIAN_DIR}/control"
    fi

    # Instalar icono SVG al staging
    local ICON_DEST="${DEB_STAGING}/usr/share/icons/hicolor/scalable/apps"
    mkdir -p "${ICON_DEST}"
    cp "${SCRIPT_DIR}/packaging/appimage/AppDir/usr/share/icons/hicolor/scalable/apps/${APPNAME}.svg" \
       "${ICON_DEST}/${APPNAME}.svg"

    # Asegurar permisos correctos
    find "${DEB_STAGING}" -type d -exec chmod 0755 {} \;
    find "${DEB_STAGING}/usr" -type f -exec chmod 0644 {} \;
    chmod 0755 "${DEB_INSTALL}/bin/${APPNAME}"

    # Construir .deb
    mkdir -p "${DIST_DIR}"
    local DEB_FILE="${DIST_DIR}/${APPNAME}_${VERSION}_$(dpkg --print-architecture 2>/dev/null || echo amd64).deb"
    fakeroot dpkg-deb --build "${DEB_STAGING}" "${DEB_FILE}"
    log "Paquete .deb generado: ${DEB_FILE}"
}

# ─────────────────────────────────────────────────────────────────────────────
# CONSTRUCCIÓN DEL APPIMAGE
# ─────────────────────────────────────────────────────────────────────────────
build_appimage() {
    log "Construyendo AppImage …"

    command -v patchelf > /dev/null 2>&1 || \
        err "'patchelf' no encontrado. Instala con: sudo apt install patchelf"

    local APPDIR="${BUILD_DIR}/AppDir"
    local SRC_APPDIR="${SCRIPT_DIR}/packaging/appimage/AppDir"

    # ── Copiar la estructura base del AppDir ──────────────────────────────────
    rm -rf "${APPDIR}"
    cp -a "${SRC_APPDIR}/." "${APPDIR}/"

    # ── Instalar el binario dentro del AppDir ─────────────────────────────────
    mkdir -p "${APPDIR}/usr/bin"
    cp "${BUILD_DIR}/${APPNAME}" "${APPDIR}/usr/bin/${APPNAME}"
    chmod +x "${APPDIR}/usr/bin/${APPNAME}"

    # ── Copiar desktop e icono al raíz del AppDir (requerido por AppImageKit) ─
    cp "${APPDIR}/admin-acts-linux.desktop" "${APPDIR}/" 2>/dev/null || true
    # Icono en la raíz del AppDir (appimagetool lo busca aquí)
    cp "${APPDIR}/usr/share/icons/hicolor/scalable/apps/${APPNAME}.svg" \
       "${APPDIR}/${APPNAME}.svg"

    # ── Empaquetar bibliotecas GTK/gtkmm necesarias ───────────────────────────
    log "Copiando bibliotecas compartidas …"
    local LIBDIR="${APPDIR}/usr/lib"
    mkdir -p "${LIBDIR}"

    # Lista de libs a empaquetar junto al binario para portabilidad
    local BUNDLE_LIBS=(
        libgtkmm-3.0.so
        libatkmm-1.6.so
        libcairomm-1.0.so
        libgiomm-2.4.so
        libglibmm-2.4.so
        libpangomm-1.4.so
        libsigc-2.0.so
        libgtk-3.so
        libgdk-3.so
        libatk-1.0.so
        libcairo.so
        libpango-1.0.so
        libpangocairo-1.0.so
        libgdk_pixbuf-2.0.so
        libglib-2.0.so
        libgobject-2.0.so
        libgio-2.0.so
        libgmodule-2.0.so
        libgthread-2.0.so
    )

    # Resolver la ruta real de cada lib y copiarla
    for libname in "${BUNDLE_LIBS[@]}"; do
        local libpath
        libpath="$(ldconfig -p 2>/dev/null | grep "${libname}" | awk '{print $NF}' | head -n 1 || true)"
        if [[ -n "${libpath}" && -f "${libpath}" ]]; then
            local reallib
            reallib="$(readlink -f "${libpath}")"
            cp -n "${reallib}" "${LIBDIR}/" 2>/dev/null || true
            # Crear enlace simbólico genérico (p.ej. libgtk-3.so → libgtk-3.so.0.2404.0)
            local basename_link
            basename_link="$(basename "${libpath}")"
            local basename_real
            basename_real="$(basename "${reallib}")"
            if [[ "${basename_link}" != "${basename_real}" ]]; then
                ln -sf "${basename_real}" "${LIBDIR}/${basename_link}" 2>/dev/null || true
            fi
        else
            warn "Biblioteca no encontrada (se omite): ${libname}"
        fi
    done

    # Ajustar rpath del binario para que apunte primero a $ORIGIN/../lib
    patchelf --set-rpath '$ORIGIN/../lib' "${APPDIR}/usr/bin/${APPNAME}" || \
        warn "patchelf falló; el AppImage puede no funcionar en otras distros."

    # ── Descargar / localizar appimagetool ────────────────────────────────────
    local APPIMAGETOOL
    if command -v appimagetool > /dev/null 2>&1; then
        APPIMAGETOOL="$(command -v appimagetool)"
    else
        APPIMAGETOOL="${BUILD_DIR}/appimagetool.AppImage"
        if [[ ! -f "${APPIMAGETOOL}" ]]; then
            log "Descargando appimagetool …"
            if command -v curl > /dev/null 2>&1; then
                curl -sSfL "${APPIMAGE_TOOL_URL}" -o "${APPIMAGETOOL}"
            elif command -v wget > /dev/null 2>&1; then
                wget -q "${APPIMAGE_TOOL_URL}" -O "${APPIMAGETOOL}"
            else
                err "Se necesita 'curl' o 'wget' para descargar appimagetool."
            fi
            chmod +x "${APPIMAGETOOL}"
        fi
    fi

    # ── Generar el AppImage ───────────────────────────────────────────────────
    mkdir -p "${DIST_DIR}"
    local APPIMAGE_FILE="${DIST_DIR}/Admin_Acts_Linux-${ARCH}.AppImage"

    # appimagetool espera ARCH en el entorno
    ARCH="${ARCH}" "${APPIMAGETOOL}" \
        --no-appstream \
        "${APPDIR}" \
        "${APPIMAGE_FILE}"

    chmod +x "${APPIMAGE_FILE}"
    log "AppImage generado: ${APPIMAGE_FILE}"
}

# ─── Ejecución principal ──────────────────────────────────────────────────────
mkdir -p "${DIST_DIR}"
check_build_deps
compile

$BUILD_DEB      && build_deb
$BUILD_APPIMAGE && build_appimage

log ""
log "══════════════════════════════════════════════"
log " Paquetes generados en: ${DIST_DIR}/"
ls -lh "${DIST_DIR}/"
log "══════════════════════════════════════════════"
