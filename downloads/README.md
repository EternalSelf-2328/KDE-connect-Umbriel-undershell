# APK experimental para controlar la computadora

- [Descargar Umbriel Remote 0.3.0](UmbrielRemote-0.3.0.apk).
- [Comprobar SHA256](SHA256SUMS).
- [Huella pública del certificado de firma](SIGNING-CERTIFICATE-SHA256).
- [Cambio de permisos y bloqueo de Play Protect](../docs/PLAY-PROTECT.md).
- [Guía de registro y límites](../docs/REMOTE-SECURITY.md).

Incluye navegación dedicada, touchpad de computadora, controles de sonido/apariencia
y galería de undershell-wall con miniaturas y filtros. Añade red local y transferencia
de archivos en ambos sentidos sin KDE Connect. [Guía actual](../docs/LOCAL-FILES.md). [Uso y límites](../docs/REMOTE-CONTROLS.md).

Esta edición permite controlar Umbriel, Noctalia y undershell desde Android,
con una sesión aprobada mediante biometría fuerte. **No puede controlar el teléfono,
leer sus ventanas ni obtener capturas.** No incluye un servicio de accesibilidad,
lector de notificaciones, permisos SMS ni código de captura/gestos del teléfono.

Compilada contra Android API 36, mínimo API 30, sin dependencias externas en el APK.
Firma APK v3 verificada, con la misma identidad de firma de 0.1.0; versionCode 4.
La clave privada está fuera del repositorio. Requiere la red privada preparada,
registro explícito, notificaciones y biometría fuerte.

La versión 0.1.0 fue bloqueada por Play Protect en el teléfono del usuario y se retiró
de la descarga recomendada. Esta versión reduce realmente sus capacidades, pero
**no tiene aprobación de Google ni instalación física verificada**. Si Play Protect
la bloquea, conserva la protección y utiliza el procedimiento oficial de revisión.
La variante con control inverso se conserva en el código, pendiente de revisión;
no se ofrece otro APK para eludir el bloqueo.

Construcción: `bash android/build.sh desktop`, resultado en
`build/android/desktop/UmbrielRemote.apk`. El SHA256 comprueba integridad, no garantiza
la seguridad de la aplicación ni sustituye la clasificación de Play Protect.
