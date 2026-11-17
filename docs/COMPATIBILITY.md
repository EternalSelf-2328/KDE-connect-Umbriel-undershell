# Funciones y verificación — 0.4.0 experimental

Actualizado el 2026-11-17. Se reemplaza el daemon y las aplicaciones KDE Connect
instaladas por motores derivados, compilados y aislados dentro de Umbriel Connect.
**Compartir su código/protocolo no significa haber validado cada función en Umbriel o
el Galaxy.** No se promete seguridad absoluta ni paridad física completa.

| Función | Implementación 0.4.0 | Límite / verificación pendiente |
|---|---|---|
| Descubrimiento LAN/mDNS/Bluetooth, emparejamiento y certificados | Motor privado C++ y app Android completa | Nueva confianza, no importa claves KDE; router/Bluetooth físicos pendientes |
| Batería y señal móvil | Plugins incluidos, ventana de funciones | Android real pendiente |
| Archivos, URL, texto en ambos sentidos | Motor con payload TLS streaming y canal firmado propio; selector/Yazi propios | Motor pone envíos en cola; confirmación SHA256 explícita pertenece al canal firmado |
| Explorador de archivos | Ventana propia con KIO privado + SSHFS, host key fijada al certificado | Requiere permiso de carpetas Android, sshfs/kio-extras; montaje físico pendiente |
| Notificaciones, acciones, respuesta y cierre | Motores incluidos, permisos Android opt-in | Respuesta afecta datos personales; sincronización al bloquear explícita |
| Portapapeles | Plugins incluidos; exportación PC desactivada inicialmente | Restricciones Android al leer en segundo plano; requiere opt-in |
| MPRIS, reproducción y volumen | Plugins completos y controles dedicados propios | Validar reproductores/Noctalia reales |
| SMS/MMS, adjuntos y contactos | Motor Android y ventana Mensajes propia | Permisos Android/operador; no se envió ningún SMS para probar |
| Llamadas y silencio/pausa | Plugins de telefonía incluidos | Depende de permisos y política Samsung; prueba física pendiente |
| Encontrar teléfono y computadora | Plugins incluidos | Necesita conexión y sesión o sincronización autorizada |
| Touchpad/teclado, presentación y tableta | Plugins incluidos; touchpad firmado propio | Input PC opt-in; portales Wayland/Umbriel reales pendientes |
| Compartir entrada por borde | Plugins incluidos, opt-in | Requiere backend de entrada/portal y accesibilidad Android; sin prueba física |
| Ejecutar órdenes | Motor privado y publicación de catálogo Umbriel/undershell | Desactivado por defecto en PC; enlaces piden confirmación; requiere sesión activa |
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
1714–1764 TCP/UDP y mDNS. No se abren puertos del router ni reglas de firewall.

El Tailscale de usuario instalado aquí funciona en **userspace**: su proxy Serve ya
permite el canal propio de controles/archivos, pero **no transporta automáticamente
todos los plugins del motor ni anuncios multicast**. SFTP, MPRIS/SMS del motor,
Bluetooth o ADB no se convierten en funciones remotas solo por estar en la tailnet.
Para el motor fuera de LAN hace falta una ruta IP real hacia ambos pares y un destino
manual; no se cambian las rutas de la sesión remota actual para conseguirlo.

## Qué comprobar en el teléfono

Instalar el APK completo con el mismo firmante conserva el canal firmado existente.
Abrir «Funciones del teléfono», aprobar huella, emparejar de nuevo el motor de la PC
comparando la clave de ambos extremos, y conceder solo los permisos necesarios.
Si se desea recepción permanente, volver a esa pantalla y marcar la computadora en
«Sincronización al bloquear»: queda ligada a su certificado, con notificación visible.
Cerrar esa sesión revoca también la sincronización; cambiar certificados no hereda permiso.

Validar envío/cancelación/archivos grandes, reconexión, consumo Samsung, notificaciones,
SMS y cámara con datos de prueba elegidos por el usuario. No se ha tomado control del
mouse, cámara, mensajes, audio o pantalla reales para estas comprobaciones.

Si Tailscale estaba detenido, el canal LAN sigue disponible. Para recuperar el modo
userspace sin cambiar DNS/rutas, deja `~/.local/bin/umbriel-remote-session vpn`
en ejecución y, en otra terminal, usa `~/.local/bin/umbriel-remote-session start`.
Este último reutiliza el receptor de la ventana si ya existe; también puede abrirse
solo el proxy con `proxy`. No vuelvas a iniciar sesión ni generes un registro si se
conserva la identidad anterior. El estado de la VPN no se cambia durante las pruebas.
