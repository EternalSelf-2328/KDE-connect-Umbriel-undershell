# Seguridad de Umbriel Connect 0.4.0 experimental

No hay garantía de ausencia de vulnerabilidades ni auditoría externa. Esta versión
incorpora una revisión dirigida del código y pruebas de regresión; no certifica el
proyecto completo ni los dispositivos. No se encontró evidencia de una puerta trasera
en las rutas examinadas; eso no demuestra su ausencia en todo el código/dependencias.

## Dos fronteras de autorización

1. **Canal propio:** TLS con certificado fijado, registro temporal de un uso, desafío,
   firma EC Android Keystore autorizada con BIOMETRIC_STRONG y secuencias. Sesión de
   control hasta10min; bloqueo, cierre y revocación cancelan. Un archivo ya aprobado
   puede terminar hasta24h con timeout de inactividad; no abre nuevas acciones.
   [Diseño previo y límites](docs/REMOTE-SECURITY.md).
2. **Motor compatible incluido:** emparejamiento manual y TLS con certificados
   persistentes. La app exige **biometría local** para una sesión interactiva de10min.
   No es una prueba biométrica remota por paquete, ni Android Key Attestation. Una
   computadora emparejada es una identidad de confianza: activa solo sus plugins.

La sincronización permanente del motor es **opcional y desactivada inicialmente**.
Se autoriza con huella, por computadora emparejada y SHA256 de su certificado. Con
pantalla bloqueada admite una lista cerrada de tipos de datos: archivos/SFTP,
notificaciones, mensajes/contactos, batería/conectividad, multimedia/volumen,
portapapeles, avisos de llamada, ping y encontrar teléfono. **Estos datos son sensibles**;
solo concede permisos y sincronización a computadoras que controlas. No admite nuevos
emparejamientos, entrada, órdenes, presentación, tableta ni monitor virtual. Exige
notificaciones habilitadas y servicio foreground; Android puede limitar su ejecución.
Enviar SMS, responder/accionar notificaciones y abrir contenido recibido exige una sesión interactiva activa. El permiso persiste hasta revocarlo, incluso tras reiniciar la app. No autoriza una
identidad nueva con el mismo nombre/ID y distinto certificado.

Al terminar la sesión interactiva se cierran payloads no autorizados, se desconectan
pares sin permiso de sincronización y se cierran las pantallas de funciones. Cerrar
sesión borra la autorización persistente, cancela transferencias y desconecta todos
los pares. Las sesiones propias del canal firmado conservan su autorización separada.

## Endurecimientos aplicados

- Motor, servicio, configuración, QML y namespace de plugins privados. Sin fallback a
  ejecutables o plugins KDE Connect instalados en el sistema; fuente upstream intacta.
- Pares LAN limitados a direcciones privadas/link-local/loopback/tailnet antes de
  procesar identidades. Ningún listener se publica mediante Funnel.
- Plugins PC de entrada, órdenes, portapapeles, bloqueo, notificaciones salientes y
  telefonía privilegiada desactivados inicialmente; opt-in revisable por dispositivo.
- SSHFS usa **StrictHostKeyChecking=yes**, archivo temporal0600 y la clave pública
  del certificado ya emparejado. Android utiliza esa misma clave para el servidor SSH.
  Un cambio de clave falla; no se acepta una clave desconocida ni se modifica ~/.ssh.
- Los enlaces navegables de órdenes en Android requieren sesión, comando registrado
  y confirmación. Los extras de notificación no aceptan un emparejamiento por sí solos.
- Compartir desde otras apps requiere URI content con ClipData y permiso de lectura
  explícito; rechaza file:// y la autoridad privada de la propia app.
- Receptores internos Android no exportados; sin backup de la app ni tráfico HTTP
  claro del motor. Se retiran READ_LOGS y ubicación de fondo. El inventario de apps
  permanece para los filtros por aplicación; instalar APK requiere decisión explícita
  y autorización Android, nunca ejecución automática de un archivo recibido.
- Abrir archivos recibidos en PC usa KIO con ejecución de programas deshabilitada.
- Monitor virtual upstream: bloqueado el backend que pasaba contraseña en argv y
  la apertura de URL arbitraria con credenciales elegida por el par. No se anuncia
  esa capacidad como disponible hasta contar con una implementación segura.
- ADB/scrcpy por argumentos separados y dispositivos autorizados; no habilita puertos
  TCP sin cifrar, root, módulos ni permisos de depuración automáticamente.
- Fuentes fijadas, checksum Gradle y verificación de dependencias Maven. Herramientas
  Android opcionales verificadas con firmas de los repositorios Arch.

Estas correcciones describen condiciones observadas en las revisiones fijadas;
no son asignaciones CVE ni afirmaciones sobre la versión más reciente de upstream.
Un plugin autorizado de órdenes/entrada/archivos conserva por diseño amplio poder
sobre el usuario. El software comprometido que ya actúa como tu usuario o controla
un dispositivo emparejado queda fuera de una promesa de aislamiento total.

## Red, distribución y reporte

No abrir 1714–1764 ni31594/31595 directamente a Internet, ni desactivar verificación de
certificados o Play Protect. LAN no necesita VPN. El Tailscale userspace actual solo
ofrece el canal propio vía proxy; no convierte todos los plugins en servicios remotos.

No publiques claves APK, Keystore, certificados privados, JSON de registro, tokens,
URLs temporales o datos personales. Para un informe, proporciona versión, ruta
afectada y un caso reproducible con datos ficticios. Los enlaces de contacto público
se limitan al repositorio; no adjuntes secretos a un issue público.
