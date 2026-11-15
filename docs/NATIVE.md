# Diseño de la ruta nativa

## Decisión

Se reutiliza `kdeconnectd` mediante su API D-Bus. Es el motor C++ oficial y ya estaba
instalado en el equipo de desarrollo. Reescribir TLS, descubrimiento, Bluetooth,
emparejamiento y todos los plugins duplicaría un motor mantenido por KDE sin aportar
integración con el compositor. El cliente nuevo no enlaza `libkdeconnectcore` ni usa
sus headers internos; depende de Qt6 Core, DBus, Network y Widgets y del paquete
KDE Connect en ejecución. Qt6 Test solo se utiliza al compilar las pruebas.

El costo añadido es un proceso de la ventana mientras está abierta y, si habilitas
el widget, un proceso C++ de escucha y una shell de supervisión compartidos entre
las barras. No se añade un segundo daemon de red. Las mediciones locales están en
[VALIDATION.md](VALIDATION.md); no son una comparación de rendimiento con Python.

## Componentes

| Archivo | Responsabilidad |
|---|---|
| `native/kdeclient.*` | Mensajes D-Bus asíncronos, dispositivos, batería, acciones y señales |
| `native/integration.*` | Catálogo de acciones, procesos por argv y socket nativo de undershell |
| `native/commandstore.*` | Órdenes remotas, fusión, respaldo, escritura y señal de actualización |
| `native/window.*` | Interfaz Qt Widgets en español, selección y revisión de controles |
| `native/main.cpp` | Ventana única y CLI sin servidor gráfico |
| `integrations/noctalia/umbriel-connect/` | Servicio de escucha y widget compartido |
| `tests/native.cpp` | Teléfono D-Bus falso, IPC temporal, CLI y publicación aislada |

La ventana y el watcher reaccionan a señales, con debounce de 100 ms. Las consultas
son asíncronas con timeout de 2,5 s. Cada consulta devuelve explícitamente un estado
disponible, parcial o fallido. Los dispositivos tienen un orden estable. No se
reintenta el descubrimiento en un bucle ni se despierta el teléfono periódicamente.

El cliente envía mensajes D-Bus sin construir `QDBusInterface`: la introspección
del daemon instalado incluye interfaces repetidas y métodos sobrecargados. La
consulta de propiedades y las llamadas directas funcionan con ese esquema.

## Integración con undershell

Se conecta con `$XDG_RUNTIME_DIR/undershell.sock`, con alternativa segura
`/run/user/<uid>`. Comprueba `SO_PEERCRED`, rechaza caracteres de control y mensajes
largos, limita la respuesta a 4 MiB y espera como máximo 2,5 s. Las acciones reciben
respuestas completas y convierten los errores del servidor en errores del cliente.

Los perfiles se consultan mediante `saves`; no se leen ni reescriben sus archivos.
Se cargan mediante `save-load ID [SALIDA|all]`. Los monitores nunca se fijan a los
conectores del equipo de desarrollo. Las acciones que persisten cambios lo hacen
mediante la API de undershell.

## Publicación de controles

`commands` es una vista previa. `publish ID` exige un dispositivo emparejado y
actualiza únicamente el espacio de claves `umbriel_connect__`. `withdraw ID` lo
retira. No se habilitan plugins ni se aceptan solicitudes automáticamente.

El formato del archivo sigue la implementación oficial de
[KdeConnectPluginConfig](https://github.com/KDE/kdeconnect-kde/blob/master/core/kdeconnectpluginconfig.cpp)
y [CommandsModel](https://github.com/KDE/kdeconnect-kde/blob/master/models/commandsmodel.cpp):
`QSettings::IniFormat`, JSON en `commands` y señal
`/kdeconnect/<id>/kdeconnect_runcommand`, `org.kde.kdeconnect.config.configChanged`.
La [ruta](https://github.com/KDE/kdeconnect-kde/blob/master/core/kdeconnectconfig.cpp)
es `$XDG_CONFIG_HOME/kdeconnect/<id>/kdeconnect_runcommand/config`.

La actualización crea un respaldo privado, conserva otras claves del INI, falla
si el JSON existente está corrupto y es idempotente. Un `QLockFile` evita carreras
entre nuestras propias instancias. La app oficial no comparte ese bloqueo: evita
ediciones simultáneas de comandos allí.

KDE ejecuta las órdenes configuradas con una shell. Por eso el binario, los IDs y
los argumentos generados se escapan individualmente con comillas POSIX; no se
interpolan nombres de dispositivos ni texto recibido desde el teléfono. El
ejecutable resultante solo expone el catálogo, sin un comando de shell arbitrario.

## Canal biométrico opcional

`connect-remote`, `umbriel-connect-remote` y `android/` implementan un canal separado
con aprobación biométrica. Tiene su propia identidad y una sesión TLS con duración
limitada; no modifica la confianza del daemon KDE. Requiere OpenSSL al compilar y
puede desactivarse con `UMBRIEL_REMOTE=OFF`. Tailscale de usuario se administra con
`umbriel-remote-session`; su cuenta y el teléfono necesitan vinculación manual.
Consulta [REMOTE-SECURITY.md](REMOTE-SECURITY.md) para el protocolo y sus límites.

## Siguientes migraciones

La interfaz nueva delega las funciones avanzadas del motor oficial a su aplicación
y a `kdeconnect-sms`. No recrea esas interfaces. El cliente original tiene funciones
adicionales que no se han traducido: scrcpy, cámara/webcam, selección de salida de
audio del teléfono y algunas extensiones de entrada/monitor. Antes de migrarlas,
deben tener pruebas aisladas y un backend de capacidades para Umbriel; no basta
con cambiar llamadas `hyprctl` por nombres parecidos.
