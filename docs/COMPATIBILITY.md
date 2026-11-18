# Funciones y verificación — 0.8.0 experimental

Actualizado el 2026-11-17. Se reemplaza el daemon y las aplicaciones KDE Connect
instaladas por motores derivados, compilados y aislados dentro de Umbriel Connect.
**Compartir su código/protocolo no significa haber validado cada función en Umbriel o
el Galaxy.** No se promete seguridad absoluta ni paridad física completa.

| Función | Implementación 0.8.0 | Límite / verificación pendiente |
|---|---|---|
| Descubrimiento LAN/mDNS/Bluetooth, emparejamiento y certificados | Motor privado C++ y app Android completa | Nueva confianza, no importa claves KDE; router/Bluetooth físicos pendientes |
| Batería y señal móvil | Plugins incluidos, ventana de funciones | Android real pendiente |
| Archivos, URL, texto en ambos sentidos | Motor con payload TLS streaming y canal firmado propio; selector/Yazi propios | Motor pone envíos en cola; confirmación SHA256 explícita pertenece al canal firmado |
| Explorador de archivos | Ventana propia con KIO privado + SSHFS, host key fijada al certificado | Requiere permiso de carpetas Android, sshfs/kio-extras; montaje físico pendiente |
| Notificaciones, acciones, respuesta y cierre | Motores incluidos, permisos Android opt-in | Respuesta afecta datos personales; LAN automática; fuera de LAN requiere sesión autorizada |
| Portapapeles | Canal propio manual en ambos sentidos, texto64KiB; plugins separados | App Android en primer plano para solicitudes desde PC; opt-in |
| MPRIS, reproducción y volumen | Plugins completos y controles dedicados propios | Validar reproductores/Noctalia reales |
| SMS/MMS, adjuntos y contactos | Motor Android y ventana Mensajes propia | Permisos Android/operador; no se envió ningún SMS para probar |
| Llamadas y silencio/pausa | Plugins de telefonía incluidos | Depende de permisos y política Samsung; prueba física pendiente |
| Encontrar teléfono y computadora | Plugins incluidos | Necesita LAN vinculada o sesión remota autorizada |
| Touchpad/teclado, presentación y tableta | Plugins incluidos; touchpad firmado propio | Input PC opt-in; portales Wayland/Umbriel reales pendientes |
| Compartir entrada por borde | Plugins incluidos, opt-in | Requiere backend de entrada/portal y accesibilidad Android; sin prueba física |
| Aplicaciones y energía propias | Catálogo .desktop/GIO, logind y confirmación de un solo uso | No ejecuta shell enviada por el teléfono; políticas locales; suspendido no recibe órdenes |
| Pantalla PC→teléfono | Captura nativa Wayland, autorización visible por monitor,5min y hasta2imágenes/s | Compositor compatible; prueba headless aprobada, Galaxy pendiente |
| Paletas, temas e idiomas | Cinco paletas, claro/oscuro/sistema; español/inglés en interfaz propia | Nombres y contenido personal conservados |
| Ejecutar órdenes | Motor privado y publicación de catálogo Umbriel/undershell | Desactivado por defecto en PC; enlaces piden confirmación; requiere LAN vinculada o sesión remota activa |
| Control/captura del teléfono | Servicio de accesibilidad único + canal firmado propio | Activación explícita en Android y sesión firmada; prueba Galaxy pendiente |
| Duplicar/controlar pantalla | Panel Teléfono C++ + scrcpy | ADB autorizado USB o inalámbrico; no requiere KDE |
| Cámara frontal/posterior | Panel Teléfono C++ + scrcpy | Android12+, 30fps; orientación automática adicional no portada |
| Webcam bajo demanda | Productor C++ con eventos v4l2loopback + scrcpy/ffmpeg | Nodo loopback existente, 720p; módulo y cámara físicos pendientes |
| Audio del teléfono en PC | BlueZ A2DP directo desde panel Teléfono | Emparejamiento/perfil independiente; no usa Tailscale |
| Monitor virtual entre computadoras | Fuente conservada, ruta insegura bloqueada | Falta backend con credencial privada y portal compatible; Android no implementa monitor virtual |
| Umbriel/Noctalia/undershell | Controles C++, socket/CLI, perfiles y galería | No se ejecutaron bloqueo/DPMS/cambios en la sesión remota del usuario |
| undershell-wall | Biblioteca, favoritos, carpetas/local/WE, miniaturas móvil | Usa CLI existente; no se cambia el fondo al explorar |
| Integración de barra | Plugin Noctalia propio opcional | No se instala el popout exclusivo de Caelestia ni se parchea su shell |
| Teclado de escritorio | Tab/MayúsTab, IJKL/flechas, Enter/Espacio, Alt1–5, CtrlK, F1 | Pruebas Qt sintéticas; respeta campos de texto y controles nativos |

## Alcance de red

En la misma LAN **no hace falta Tailscale**. El canal firmado propio usa TLS31594 y
anuncios UDP31595. El motor compatible usa su descubrimiento y conexiones privadas
1714–1764 TCP/UDP y mDNS. El instalador no abre puertos del router ni modifica el firewall. Si este bloquea
entradas, requiere permitir los puertos anteriores desde la LAN; la regla antigua
1714–1764 no incluye31594/31595. [Comprobar y corregir](FIREWALL.md).

El Tailscale de usuario instalado aquí funciona en **userspace**: su proxy Serve ya
permite el canal propio de controles/archivos, pero **no transporta automáticamente
todos los plugins del motor ni anuncios multicast**. SFTP, MPRIS/SMS del motor,
Bluetooth o ADB no se convierten en funciones remotas solo por estar en la tailnet.
Para el motor fuera de LAN hace falta una ruta IP real hacia ambos pares y un destino
manual; no se cambian las rutas de la sesión remota actual para conseguirlo.

## Qué comprobar en el teléfono

Instalar el APK completo con el mismo firmante conserva los datos. El registro antiguo
puede pedir una última huella para migrar las claves; después, la LAN directa se
reconecta sola y fuera de ella ofrece huella o PIN/patrón/contraseña del teléfono.
Abrir **Ajustes → Dispositivos y funciones** y emparejar el motor propio comparando las claves en
ambos extremos. Conceder solo los permisos necesarios. La sincronización remota
persistente anterior se retira; «Pausar conexión» suspende la reconexión local.

Validar envío/cancelación/archivos grandes, reconexión, consumo Samsung, notificaciones,
SMS y cámara con datos de prueba elegidos por el usuario. No se ha tomado control del
mouse, cámara, mensajes, audio o pantalla reales para estas comprobaciones.

Si Tailscale estaba detenido, el canal LAN sigue disponible. Para recuperar el modo
userspace sin cambiar DNS/rutas, deja `~/.local/bin/umbriel-remote-session vpn`
en ejecución y, en otra terminal, usa `~/.local/bin/umbriel-remote-session start`.
Este último reutiliza el receptor de la ventana si ya existe; también puede abrirse
solo el proxy con `proxy`. No vuelvas a iniciar sesión ni generes un registro si se
conserva la identidad anterior. El estado de la VPN no se cambia durante las pruebas.

[Correcciones y verificaciones 0.4.1](UPDATE-0.4.1.md). El cierre del Galaxy aún no
se ha reproducido con un teléfono conectado: se corrigió una incompatibilidad real
del servicio y se añadieron pruebas de regresión y un informe privado de errores.

0.4.2 corrige el NoSuchFieldError reportado al vincular y valida el bytecode release.
Los anuncios sin vínculo se muestran como disponibles. [Detalle](UPDATE-0.4.2.md).


## Interfaz Android0.5.0

Inicio propio con controles directos; Control, Archivos y Ajustes independientes.
Los permisos/plugins se administran desde Ajustes; no se abre el drawer heredado al
iniciar o tocar la notificación. El backend y las pantallas especializadas de algunos
plugins siguen derivando del motor; no todas se han rediseñado. El registro inicial
admite QR/archivo privado y confirmación; no fusiona las dos identidades de protocolo.
31 pruebas Android y10 suites C++ aprobadas; renders nativos sintéticos en claro/
oscuro,320/412/840 dp y texto 130%. Samsung físico pendiente. [Actualización](UPDATE-0.5.0.md).


## Conexión y escritorio0.6.0

Primera vinculación LAN por descubrimiento y comparación de código, sin importarJSON;
requiere confirmación una vez en la PC y prueba de posesión de claves. Después conserva
la LAN automática y autorización remota. Olvidar revoca el grupo exacto, no otros
dispositivos. Anuncios sin vínculo pueden ocultarse y restaurarse. Nuevo menú lateral
y configurador de funciones propio Qt. [Uso y límites](UPDATE-0.6.0.md).


## Corrección Android0.6.1

Escritorio0.6.0 compatible; canales compartidos conservados al arrancar, espaciado
entre pestañas/touchpad/botones y márgenes verificados en37pruebas Android. Render
API36 con datos sintéticos y densidad1.5; Samsung físico pendiente.
[Diagnóstico y actualización](UPDATE-0.6.1.md).

## Reproductor y salida de audio 0.7.0

El panel propio Android consulta MPRIS y permite elegir el reproductor. Título,
artista, aplicación, dominio y carátula dependen de los datos publicados por cada
aplicación. El selector de salida requiere libpulse al compilar y un servidor
PulseAudio o PipeWire-Pulse; cambia el destino predeterminado y solicita mover los
flujos actuales, con aviso si hay fallos parciales. Pruebas con salidas nulas y
reproductor sintético; Galaxy y dispositivos físicos pendientes.
[Detalles y restricciones](UPDATE-0.7.0.md).

[Nuevos controles y límites0.8.0](UPDATE-0.8.0.md).
