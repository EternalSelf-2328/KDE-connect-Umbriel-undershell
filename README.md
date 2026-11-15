# Umbriel Connect

Cliente nativo **C++20 / Qt6** para integrar KDE Connect con **Umbriel, Noctalia y
undershell**. Este fork proviene de [Caelestia Connect](https://github.com/maylton/caelestia-connect).

La ruta nativa usa el daemon oficial `kdeconnectd`, escrito en C++, mediante D-Bus.
No necesita Plasma ni un segundo daemon de protocolo. La aplicación, el CLI y los
controles de escritorio nuevos están en `native/`; no requieren Python.

**Estado: 0.1.0, migración inicial.** No es una traducción completa de las 14.000
líneas del cliente original. Su código y sus extras se conservan en
`caelestia_connect/`. Cámara, scrcpy y monitor virtual aún requieren trabajo para
ofrecerse desde la interfaz nativa. Consulta la [matriz de compatibilidad](docs/COMPATIBILITY.md).

## Funciones nativas

- Lista de dispositivos, batería, conexión y solicitudes de emparejamiento por eventos.
- Emparejar, aceptar solicitudes con clave visible y desvincular.
- Enviar archivos, enlaces y texto; encontrar el teléfono y explorar sus archivos.
- Controles de escritorios, foco, vista general y DPMS de Umbriel.
- Volumen, bloqueo, reposo, no molestar y tema claro/oscuro de Noctalia.
- Editor, visibilidad de widgets, demo, salvapantallas, cinco niveles de rendimiento,
  seis reacciones de mascotas y perfiles guardados de undershell.
- Publicar estos controles en **Ejecutar órdenes** del teléfono, con respaldo y
  conservando las órdenes personales. Se pueden retirar por separado.
- Widget opcional de Noctalia: estado y batería en el tooltip; clic para abrir.
- Una sola ventana por sesión. CLI sin dependencia de un servidor gráfico.

`Mostrar todos los widgets` hace visibles también los que ocultaste individualmente;
ambas acciones de visibilidad persisten mediante el IPC de undershell.

## Instalar

En Arch/CachyOS:

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-wayland kdeconnect
./install.sh
~/.local/bin/umbriel-connect
```

El instalador compila, ejecuta las pruebas en un bus privado e instala en `~/.local`.
No cambia atajos, firewall, servicios, emparejamientos ni ajustes del escritorio.
Para otro prefijo: `./install.sh --prefix /ruta`.

`umbriel`, `noctalia` y `undershell` son integraciones opcionales. Si falta alguno,
la aplicación sigue administrando el teléfono. Busca los ejecutables también en
`~/.local/bin`, aunque el compositor no incluya esa carpeta en `PATH`.

Para una instalación de sistema o paquetes:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
DESTDIR="$PWD/staging" cmake --install build
```

## Controles desde Android

1. Empareja el teléfono con el KDE Connect oficial de la computadora.
2. Habilita **Ejecutar órdenes** en los ajustes de plugins de KDE Connect.
3. Abre Umbriel Connect y selecciona el teléfono.
4. Presiona **Configurar controles remotos…**, revisa la lista y aplica.
5. En Android, abre **Ejecutar órdenes**. Si agregas perfiles de undershell,
   vuelve a publicar para actualizar la lista.

También puedes revisar y publicar desde el CLI:

```bash
~/.local/bin/umbriel-connect list
~/.local/bin/umbriel-connect commands
~/.local/bin/umbriel-connect publish ID_DEL_TELEFONO
~/.local/bin/umbriel-connect withdraw ID_DEL_TELEFONO
```

Se guarda una copia `config.bak-umbriel-connect-*` junto a la configuración del
plugin. Solo se administran claves JSON con prefijo `umbriel_connect__`. El
archivo se escribe con `QSettings` de forma atómica y se emite la misma señal
`configChanged` que usa KDE; no se reinicia el daemon. El bloqueo de nuestra
aplicación evita dos publicaciones simultáneas desde Umbriel Connect. Evita editar
las órdenes en la app oficial mientras publicas: la app oficial no comparte ese bloqueo.

## Widget de Noctalia

Requiere API 45 o posterior. Una vez instalado el cliente:

```bash
noctalia msg plugins source add umbriel-connect-local path ~/.local/share/umbriel-connect/noctalia
noctalia msg plugins enable eternalself-2328/umbriel-connect
```

Después agrega **Umbriel Connect** desde los ajustes de widgets de la barra. El
plugin comparte un único proceso C++ `watch` entre todas las barras, además de una
shell de supervisión. No hace sondeo periódico; al salir el proceso, limpia el estado
y reintenta después de tres segundos. La ruta del ejecutable se puede configurar.

No cambia automáticamente tus barras. Para desactivarlo:

```bash
noctalia msg plugins disable eternalself-2328/umbriel-connect
```

## CLI y atajos

```bash
umbriel-connect --help
umbriel-connect list
umbriel-connect watch
umbriel-connect share ID /ruta/al/archivo
umbriel-connect text ID 'Un texto para el teléfono'
umbriel-connect actions
umbriel-connect profiles
umbriel-connect action undershell.edit-on
umbriel-connect action undershell.perf-eco
umbriel-connect action undershell.pet-dance
umbriel-connect action undershell.profile ID_DEL_PERFIL
umbriel-connect action umbriel.overview-toggle
```

En los atajos del compositor, usa la ruta absoluta al ejecutable. El catálogo
`actions` indica disponibilidad de las dependencias; la ejecución valida errores.
Los mensajes enviados por el socket de undershell tienen límite, timeout y control
del propietario. Una respuesta de error produce un código de salida distinto de cero.
Los argumentos de teléfono se pasan por D-Bus; los programas de escritorio se
invocan mediante `argv`, sin interpretar texto del teléfono como una orden de shell.

## Arquitectura y comprobaciones

- [Arquitectura, decisiones y migración](docs/NATIVE.md).
- [Compatibilidad, pruebas reales y pendientes](docs/COMPATIBILITY.md).
- [Documentación original de Caelestia](docs/CAELESTIA.md).
- [Resultados de validación](docs/VALIDATION.md).
- [Diseño de acceso remoto y biometría — aún no habilitado](docs/REMOTE-SECURITY.md).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
noctalia plugins lint integrations/noctalia/umbriel-connect
QT_QPA_PLATFORM=offscreen build/umbriel-connect --preview build/preview.png
```

Las pruebas nativas usan un bus D-Bus privado, un teléfono falso y sockets/configs
temporales. No debes probar DPMS, bloqueo, red o escritura de widgets en tu sesión
real mientras trabajas de forma remota.

La ruta heredada se instala explícitamente con `bash scripts/install-legacy.sh
--no-shell`; no ejecutes simultáneamente ambos daemons de protocolo. Sus
emparejamientos son independientes y sus adaptaciones exclusivas de Hyprland siguen
pendientes. El instalador original ya no es la instalación predeterminada del fork.

## Licencia y créditos

GPL-3.0-or-later. Cliente original: Caelestia Connect y sus colaboradores. Motor
nativo: [KDE Connect](https://invent.kde.org/network/kdeconnect-kde). Integración con
Umbriel y undershell: EternalSelf-2328. Se conservan los avisos de autoría del proyecto original.
