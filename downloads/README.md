# APK experimental para Android

- [Descargar Umbriel Remote 0.1.0](UmbrielRemote-0.1.0.apk).
- [Comprobar SHA256](SHA256SUMS).
- [Guía de registro y límites](../docs/REMOTE-SECURITY.md).

Compilado contra Android API 36, mínimo API 30, sin dependencias externas en el APK.
Firma APK v3 verificada con las herramientas oficiales de Android. La clave privada
está fuera del repositorio. Requiere Tailscale o la red privada preparada, registro
explícito, notificaciones y biometría fuerte; accesibilidad solo si deseas controlar
el teléfono. No hay prueba física todavía ni auditoría de seguridad externa.

El archivo fue construido desde `android/` mediante `android/build.sh`. El hash es
una comprobación de integridad; descarga únicamente del repositorio que reconozcas.
