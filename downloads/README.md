# Aplicaciones Android

- **[Umbriel Connect0.4.0 completo](UmbrielConnect-0.4.0.apk)**: motor de funciones,
  mensajes/notificaciones/contactos/archivos/multimedia y control inverso opcional,
  además del dashboard y canal firmado de Umbriel Remote. versionCode6, API37/min30; experimental, prueba Galaxy pendiente.
- **[Umbriel Remote0.3.1 mínimo](UmbrielRemote-0.3.1.apk)**: controles de computadora,
  galería y archivos propios; sin SMS, lector de notificaciones ni accesibilidad.

Son alternativas con el mismo paquete `org.eternalself.umbrielremote` y firmante.
0.4.0 actualiza la anterior conservando sus datos; una bajada de versión no se instala
normalmente sobre una versión superior. No borres los datos para probar sin respaldar
antes tus registros/decisiones de confianza por un medio privado.

[SHA256](SHA256SUMS) · [Certificado de firma](SIGNING-CERTIFICATE-SHA256) ·
[Permisos y seguridad](../SECURITY.md) · [Play Protect](../docs/PLAY-PROTECT.md).

0.4.0 incluye capacidades sensibles que requieren permisos Android por función y
biometría local. La sincronización al bloquear es opcional, ligada al certificado de
la computadora y con notificación visible. El canal propio mantiene firmas por sesión;
el motor compatible usa TLS emparejado y autorización local, sin atestación remota.
La firma valida identidad e integridad; no acredita seguridad ni aprobación de Google.
No desactives Play Protect si impide instalarla. No hay instalación física confirmada.

La clave de publicación queda fuera del repositorio. Para reconstruir y firmar tu
propio fork, consulta [BUILDING](../docs/BUILDING.md). Las fuentes completas están
fijadas en submódulos y las adaptaciones son reproducibles; no se distribuye una clave
privada ni un registro de dispositivo.
