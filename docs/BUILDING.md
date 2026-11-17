# Compilar Umbriel Connect 0.4.0

El cliente, receptor y motor de escritorio están en C++20/Qt6. El motor se compila desde
una revisión de KDE Connect vendorizada como submódulo; **no utiliza el programa
`kdeconnectd`, su servicio D-Bus ni sus plugins instalados en el sistema**. Qt y KDE
Frameworks siguen siendo bibliotecas necesarias. Android combina el motor Kotlin/Java
con el canal propio Java; no se ha traducido Android a C++.

```bash
git clone --recurse-submodules https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell.git
cd KDE-connect-Umbriel-undershell
```

En Arch/CachyOS, instalar las bibliotecas de desarrollo (el instalador no ejecuta sudo):

```bash
sudo pacman -S --needed base-devel cmake extra-cmake-modules gettext python \
  qt6-base qt6-declarative qt6-multimedia qt6-connectivity qt6-tools qt6-wayland \
  kconfig kcoreaddons kcrash kdbusaddons kdeclarative kguiaddons ki18n kio \
  kirigami kirigami-addons kitemmodels knotifications kpeople kservice \
  kstatusnotifieritem kwindowsystem kwidgetsaddons kpackage solid qqc2-desktop-style \
  pulseaudio-qt modemmanager-qt libei libevdev libfakekey libx11 libxkbcommon \
  libxtst wayland wayland-protocols openssl libwebp
./install.sh
~/.local/bin/umbriel-connect
```

`install.sh` verifica los dos grupos de pruebas C++, respalda los binarios anteriores y
actualiza el prefijo `~/.local`. No importa certificados de KDE, no elimina paquetes,
no instala autostart, no modifica firewall/rutas/DNS ni reinicia ventanas o servicios.
El motor propio se activa al abrir la aplicación o por su servicio D-Bus privado.
Si tienes una instancia anterior abierta, ciérrala y vuelve a abrirla.

Para empaquetar: `./install.sh --prefix /ruta/absoluta`. Para separar compilación e
instalación: `scripts/engine/build.sh /prefijo`, `tests/engine.sh`, CMake raíz y `ctest`;
instalar ambos proyectos con `cmake --install build/engine --strip` y
`cmake --install build --strip`. `DESTDIR` funciona en ambas instalaciones.

Rutas privadas:

- Motor: `PREFIX/lib/umbriel-connect/engine/`.
- Plugins: `lib/plugins/umbriel-connect/engine/` dentro del motor.
- Servicio: `org.eternalself.UmbrielConnect.Engine`.
- Confianza/configuración: `~/.config/umbriel-connect/engine/`.
- Canal firmado anterior: conserva sus datos y su certificado, separado del motor.

Los nombres de paquetes de protocolo, interfaces D-Bus y módulos QML `kdeconnect`
se conservan por compatibilidad; no activan el daemon oficial. No ejecutes el
instalador Python heredado para instalar la versión nueva.

## Android completo

Se requiere Linux x86_64, JDK21 y SDK37. Se mantienen los SDK/JDK fuera del repositorio.
`UMBRIEL_JDK` y `UMBRIEL_ANDROID_TOOLS` permiten cambiar sus rutas. El directorio de
JDK predeterminado es `~/.cache/umbriel-connect-android/jdk-21.0.13+9`.

```bash
# Si no hay JDK: descarga herramientas oficiales verificadas (también conserva SDK36 mínimo).
python3 scripts/setup-android-tools.py
# Instala SDK37 mediante herramientas oficiales, con checksum y sin métricas.
python3 scripts/android-full/sdk.py
scripts/android-full/build.sh
# Pruebas Android con recursos (Robolectric):
scripts/android-full/build.sh testDebugUnitTest --tests org.eternalself.umbrielremote.IncomingShareTest
bash tests/android-guard.sh
```

El APK release resultante es **sin firmar** y está bajo
`build/android-full-source/build/outputs/apk/release/`. Dependencias Maven y Gradle
usan revisiones y verificaciones SHA256; una actualización exige revisar los cambios
y actualizar `data/android-full-verification-metadata.xml`, no desactivar la comprobación.

Para firmar tu propio fork, crea y respalda tu propia clave fuera de Git. Configura
`UMBRIEL_ANDROID_KEYSTORE`, `UMBRIEL_ANDROID_KEY_PASSWORD_FILE` y
`UMBRIEL_ANDROID_KEY_ALIAS`, y ejecuta `scripts/android-full/sign.sh /ruta/salida.apk`.
No se distribuye una clave de firma. Solo las actualizaciones del mismo firmante pueden
reemplazar una app ya instalada sin borrar sus datos.

## Herramientas opcionales

- Exploración SFTP: `sshfs`, `kio-extras` y OpenSSH. La clave SSH debe coincidir con
  el certificado del teléfono; no hay aceptación de claves desconocidas.
- Pantalla/cámara: `scrcpy`, `android-tools`; autorización ADB separada y explícita.
  `python3 scripts/tools/install.py` instala paquetes Arch firmados en el prefijo
  privado como `umbriel-adb`/`umbriel-scrcpy`, sin servicios ni cambios USB.
- Cámara bajo demanda: `ffmpeg` y un dispositivo `v4l2loopback` creado por el
  administrador, con los headers del kernel correcto. No se carga un módulo desde la app.
- Audio: teléfono emparejado por BlueZ, perfil A2DP y PipeWire habilitado.

No se configura ADB TCP sin cifrar, no se carga `uinput` ni se añade el usuario a grupos
privilegiados. Conexión inalámbrica ADB: usa el emparejamiento seguro de Android y
`umbriel-adb pair IP:PUERTO`, seguido de `umbriel-adb connect IP:PUERTO` cuando corresponda.
El código de emparejamiento se escribe en la entrada interactiva de ADB, nunca en argv.

Si el KDE Connect oficial sigue iniciándose por otro componente, puede mantenerse
instalado mientras terminas el emparejamiento nuevo. Para retirarlo de tu inicio,
usa una anulación de usuario de su entrada XDG autostart con `Hidden=true`; respáldala
antes. No borres sus bibliotecas de Qt/KDE: Umbriel las usa independientemente del
daemon oficial. El instalador no altera esa decisión automáticamente.
