# Seguridad de Umbriel Connect 0.6.0 experimental

No hay garantía de ausencia de vulnerabilidades ni auditoría externa. Esta versión
incorpora una revisión dirigida del código y pruebas de regresión; no certifica el
proyecto completo ni los dispositivos. No se encontró evidencia de una puerta trasera
en las rutas examinadas; eso no demuestra su ausencia en todo el código/dependencias.

## Red local y acceso remoto

**La LAN omite la confirmación humana, pero mantiene la autenticación del dispositivo.**
Solo funcionan identidades previamente vinculadas y TLS verificado. No se concede
confianza por nombre de Wi-Fi, nombre del teléfono ni anuncios de descubrimiento.

1. **Canal propio:** certificado TLS fijado, registro temporal de un uso y firma EC
   Android Keystore. Hay claves separadas para LAN automática y acceso remoto. La
   firma v2 liga modo, capacidades, ambas claves, certificado y nonce al mismo socket.
   La clave LAN no puede autorizar sesiones remotas ni el protocolo v1. La PC exige
   una ruta directa RFC1918 por una interfaz física activa y el mismo prefijo; Android
   exige Wi-Fi/Ethernet real sin transporte VPN y enlaza el socket a esa red.
2. **Motor compatible incluido:** emparejamiento manual y TLS con certificados
   persistentes. Android comprueba el enlace TLS de cada par: en LAN directa un par
   emparejado usa los plugins habilitados; por otra ruta necesita una sesión local
   de identidad de hasta10 min. Ese permiso es una barrera local, no una prueba remota
   por paquete ni Android Key Attestation.

Fuera de LAN, las claves nuevas requieren por uso **BIOMETRIC_STRONG o DEVICE_CREDENTIAL**:
la huella fuerte o el PIN, patrón o contraseña del teléfono. No hay contraseña guardada
por Umbriel, biometría débil ni token remoto permanente. Bloquear el teléfono revoca
el control remoto. Solo los archivos remotos ya aprobados pueden finalizar dentro
del alcance y plazo limitado de transferencia. [Protocolo](docs/REMOTE-SECURITY.md).

En LAN, la conexión y los archivos pueden continuar al bloquear o salir de la app,
con servicio foreground y notificación visible. Los archivos de la computadora
vinculada se guardan en Descargas, sin abrirse automáticamente. El control/captura
por accesibilidad sigue exigiendo teléfono desbloqueado y permiso explícito. Los
plugins habilitados pueden intercambiar datos sensibles: concede solo los que uses.
«Pausar conexión» cancela y suspende la reconexión, también después de reiniciar.

Los permisos remotos persistentes de 0.4.0 se borran al iniciar. Una clave antigua
limitada a huella necesita una última firma para autorizar las claves nuevas; un
registro nuevo es la alternativa cuando esa huella ya no se puede usar. Revocar la
identidad principal invalida sus claves delegadas y evita que una clave de migración
restaure una identidad revocada. No restaures el receptor 0.4.0 con un almacén de
confianza v2: restaura también la copia privada anterior a esa migración.

Las comprobaciones de ruta son conservadoras, no una demostración criptográfica de
ubicación física. Un router comprometido, NAT que presente tráfico remoto como local
o un dispositivo emparejado comprometido pueden romper esa suposición. No publiques
el receptor mediante proxies públicos ni redirijas sus puertos en el router.

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
- Servicio Android de conexión con tipo `connectedDevice` y sus permisos declarados;
  errores de arranque controlados y reporte privado acotado sin mensajes de excepción.
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

## Corrección de compatibilidad0.4.2

El informe Galaxy de 0.4.1 identificó un NoSuchFieldError de red, causado por R8 al
usar una clase duplicada de la dependencia MMS. Se conserva la frontera del API
Android y se verifica el bytecode release; no se debilita la clasificación LAN ni
la autenticación para ocultar el error. [Diagnóstico](docs/UPDATE-0.4.2.md).
Los anuncios no vinculados se muestran como disponibles, sin afirmar conexión o
confianza; no se fusionan identidades solo por nombre o IP.

## Registro desde QR/enlace en 0.5.0

El URI exportado `umbriel-connect://pair` solo propone un registro: no autoriza por
recibir un intent. Se limita el tamaño, formato, host privado/Tailscale, puerto,
certificado, token y caducidad; se vuelve a comprobar al confirmar. Cancelar conserva
la confianza anterior. Cambiar a un certificado distinto retira el permiso opt-in de
controlar el teléfono. Los estados visuales de conexión no conceden autorización.

El QR se genera localmente mediante una biblioteca opcional; su contenido no sale a
un generador web. El archivo de exportación tiene permisos0600 y el portapapeles se
limpia al caducar si su contenido sigue siendo el copiado. Eso no revoca copias que
el usuario haya hecho: el límite de uso/caducidad también se comprueba en el receptor.

## Vinculación LAN por código —0.6.0

La primera confianza puede aprobarse sin transferir un JSON. El canal TLS candidato
no es confiable hasta comparar el código de ambas pantallas y aprobar en la PC por
su socket Unix privado. La prueba P-256 y el código ligan certificado, ambas claves
y nonce, evitando delegar claves a otro endpoint mediante una respuesta de descubrimiento
suplantada. No se hereda el permiso de controlar el teléfono ni se modifican claves
existentes; hay límites de solicitudes y caducidad. El acceso remoto sigue exigiendo
la clave protegida por identidad Android. [Protocolo y uso](docs/UPDATE-0.6.0.md).

Olvidar revoca el grupo exacto y sus sesiones; no usa nombres/IP para decidir qué
confianza eliminar. Los anuncios ocultos se pueden restaurar desde Ajustes.

## Canales Android0.6.1

La inicialización del motor conserva los canales de notificación ajenos a su módulo,
incluido el de la sesión propia. Se elimina el borrado global que provocaba un
SecurityException cuando el canal estaba asociado a un servicio foreground activo.
Las notificaciones/permisos no se ocultan ni se evitan; el usuario conserva sus
ajustes existentes. La regresión se verifica también en el bytecode release.
