# Aplicaciones Android

- **[Umbriel Connect 0.8.0 completo](UmbrielConnect-0.8.0.apk)**: selección múltiple de archivos, inglés, paletas y temas, texto/teclado y
  portapapeles en ambos sentidos, aplicaciones y energía, vista de pantalla con
  autorización local; reproductor con carátula, título/origen y estado real, salida de audio,
  inversión del cursor y desplazamiento, menos espera de movimiento; vínculo LAN por código, Inicio con controles
  directos, navegación inferior, Archivos y Ajustes separados; LAN automática tras
  vincular y autorización remota con huella o PIN/patrón/contraseña. Incluye el arreglo
  del NoSuchFieldError de 0.4.2. versionCode 13, API 37/min 30; prueba Galaxy pendiente.
- **[Umbriel Remote 0.3.1 mínimo](UmbrielRemote-0.3.1.apk)**: edición histórica sin SMS,
  lector de notificaciones ni accesibilidad. No incorpora las correcciones posteriores.

Comparten paquete `org.eternalself.umbrielremote` y firmante. 0.8.0 actualiza las
versiones anteriores conservando datos. No borres los datos ni desinstales para
actualizar. Una bajada de versión normalmente no se instala sobre una superior.
Los APK completos anteriores se conservan en el historial de Git.

[Pasos de actualización](../docs/UPDATE-0.8.0.md) · [SHA256](SHA256SUMS) ·
[Certificado de firma](SIGNING-CERTIFICATE-SHA256) ·
[Permisos y seguridad](../SECURITY.md) · [Play Protect](../docs/PLAY-PROTECT.md).

Los permisos Android son opcionales por función. En LAN los dispositivos vinculados
se autentican sin pedir huella y mantienen una notificación visible; fuera de ella
requieren identidad por hasta 10 min. Un registro antiguo puede pedir una última
huella para migrar sus claves. No hay atestación remota de Keystore.

La firma valida identidad e integridad; no acredita seguridad ni aprobación de Google.
No desactives Play Protect si impide instalarla. La clave de publicación está fuera
del repositorio. [Reconstruir y firmar tu fork](../docs/BUILDING.md).
