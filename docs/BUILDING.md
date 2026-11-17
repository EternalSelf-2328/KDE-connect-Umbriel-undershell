# Compilar Umbriel Connect 0.7.0

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
no modifica firewall/rutas/DNS ni reinicia ventanas o servicios.
`./install.sh --autostart` habilita los servicios de usuario para la siguiente sesión
gráfica; sin esa opción se conserva el inicio por GUI/D-Bus.
El motor propio se activa al abrir la aplicación o por su servicio D-Bus privado.
Si tienes una instancia anterior abierta, ciérrala y vuelve a abrirla.

`libpulse` es opcional para el selector nativo de salidas PulseAudio/PipeWire-Pulse.
Las pruebas de audio necesitan además `pipewire`, `pipewire-pulse`, `wireplumber`,
`pactl` y, para verificar una reproducción activa, `pacat`; usan un servidor privado.

`prison` es opcional: genera el QR de vinculación dentro del proceso, sin servicios
web. Sin ella, el diálogo conserva exportación de archivo y portapapeles privados.
La prueba de lectura del QR usa `KF6PrisonScanner` cuando está disponible.

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
bash scripts/android-full/test.sh
bash tests/android-guard.sh
```

El APK release resultante es **sin firmar** y está bajo
`build/android-full-source/build/outputs/apk/release/`. Dependencias Maven y Gradle
usan revisiones y verificaciones SHA256; una actualización exige revisar los cambios
y actualizar `data/android-full-verification-metadata.xml`, no desactivar la comprobación.

`HomeRenderTest` dibuja las Views reales con datos sintéticos y runtime gráfico nativo
Robolectric, en API 36: claro/oscuro, ancho320/412/840 dp y texto aumentado al130%.
El script selecciona las clases del fork; no presenta como validado el conjunto de
pruebas unitarias heredadas, que tiene su propio arnés y stubs de Android.
Los PNG quedan en `build/android-home-previews/`; no se captura el escritorio ni un
teléfono real. Su dependencia de fuentes/runtime nativo suma unos152 MiB a la caché
de pruebas; no entra en el APK. El checksum está fijado en los metadatos de verificación.

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

## Servicios de usuario

```bash
systemctl --user daemon-reload
systemctl --user enable umbriel-connect-engine.service umbriel-connect-remote.service
# Para iniciarlos ahora, primero cerrar las instancias independientes anteriores:
systemctl --user start umbriel-connect-engine.service umbriel-connect-remote.service
systemctl --user is-active umbriel-connect-engine.service umbriel-connect-remote.service
```

Se instalan bajo `PREFIX/share/systemd/user/`; el inicio requiere que el prefijo esté
en las rutas de búsqueda del systemd de usuario (por defecto `~/.local/share`);
`install.sh --autostart` enlaza las unidades desde el prefijo elegido. No
se inicia Tailscale. El receptor solo ofrece cierre mediante su socket Unix privado
con verificación de UID; no admite cierre administrativo desde la conexión TLS.
Si existe una sesión/transferencia activa, esperar a que termine antes de sustituir
procesos. Reabrir la GUI antigua para cargar sus nuevos textos y estados.

Al retroceder a un receptor anterior a0.4.1, restaurar también el `clients.json`
anterior a v2 desde el respaldo privado del instalador. El receptor antiguo no conoce
los alcances de las claves LAN. Nunca copiar ese archivo a Git o documentación pública.

Después de firmar el APK completo, `python3 tests/android-full-artifacts.py` inspecciona
el manifiesto fusionado y DEX, firma, versión y checksum del artefacto publicado.


## Evitar la reactivación del daemon oficial

Si elegiste sustituirlo completamente, respalda primero cualquier anulación de
usuario existente y espera a que finalicen sus transferencias. Además del XDG
autostart `Hidden=true`, la activación D-Bus puede volver a iniciarlo. Una anulación
opcional de usuario se incluye en `data/org.kde.kdeconnect.disabled.service`:

```bash
install -Dm644 data/org.kde.kdeconnect.disabled.service "$HOME/.local/share/dbus-1/services/org.kde.kdeconnect.service"
# Detener solo el daemon oficial identificado, sin reiniciar el bus de sesión.
busctl --user call org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus ReloadConfig
```

No se instala esa anulación automáticamente en otros equipos. No redirige confianza
a Umbriel y no desinstala las bibliotecas. Para revertir, restaurar la anulación
anterior o retirar solo este archivo si no existía y recargar la configuración D-Bus.

## Firewall local

El instalador no abre puertos. Si usas UFW u otro firewall con entrada bloqueada,
permite UDP31595/TCP31594 únicamente desde tu LAN; el permiso del motor1714–1764
es independiente. [Diagnóstico, perfiles opcionales y reversión](FIREWALL.md).
