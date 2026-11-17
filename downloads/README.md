# Aplicaciones Android

- **[Umbriel Connect 0.4.1 completo](UmbrielConnect-0.4.1.apk)**: conexión LAN automática,
  autorización remota con huella o PIN/patrón/contraseña y corrección del servicio que
  podía cerrar la app. versionCode7, API37/min30; prueba física del Galaxy pendiente.
- **[Umbriel Remote 0.3.1 mínimo](UmbrielRemote-0.3.1.apk)**: edición histórica sin SMS,
  lector de notificaciones ni accesibilidad. No incorpora las correcciones 0.4.1.

Comparten paquete `org.eternalself.umbrielremote` y firmante.0.4.1 actualiza 0.4.0 sin
borrar sus datos. Una bajada de versión normalmente no se instala sobre una superior.
No borres los datos ni desinstales para actualizar. La descarga 0.4.0 con el fallo de
servicio se retira de esta carpeta; permanece en el historial del repositorio.

[Pasos de actualización](../docs/UPDATE-0.4.1.md) · [SHA256](SHA256SUMS) ·
[Certificado de firma](SIGNING-CERTIFICATE-SHA256) ·
[Permisos y seguridad](../SECURITY.md) · [Play Protect](../docs/PLAY-PROTECT.md).

Los permisos Android siguen siendo opcionales por función. En LAN los dispositivos
vinculados se autentican sin pedir huella y mantienen una notificación visible;
fuera de ella requieren identidad por hasta10 min. Un registro antiguo puede pedir
una última huella para migrar sus claves. No hay atestación remota de Keystore.

La firma valida identidad e integridad; no acredita seguridad ni aprobación de Google.
No desactives Play Protect si impide instalarla. La clave de publicación está fuera
del repositorio. [Reconstruir y firmar tu fork](../docs/BUILDING.md).
