> Sustituida por [0.4.2](UPDATE-0.4.2.md): un informe del Galaxy identificó otro fallo en el APK optimizado.

# Actualización 0.4.1 — conexión local y autenticación remota

**2026-11-17 · experimental · Android versionCode7.**

## Qué cambia

- Corregido el manifiesto del servicio de conexión: ahora declara `connectedDevice`
  y los permisos exigidos por Android. Antes podía cerrarse al llamar `startForeground`.
  Los fallos de inicio y de apertura del diálogo ahora se muestran en la interfaz.
- El diálogo de identidad ofrece huella fuerte o el PIN, patrón o contraseña del
  teléfono. Se evita cancelar la autenticación al pasar a la pantalla del bloqueo.
- La LAN directa usa una clave de dispositivo separada y conexión automática con
  notificación visible. Fuera de ella hace falta autorización por hasta diez minutos.
- Reconexión local en segundo plano, tras iniciar el teléfono o actualizar. Al vencer
  la sesión remota se espera una LAN, sin autorizar automáticamente el acceso remoto.
  «Pausar conexión» detiene esa espera hasta presionar Conectar.
- El selector de archivos conserva una sesión válida al regresar. La recepción LAN
  guarda archivos sin abrirlos; la remota mantiene confirmación por archivo.
- Servicios de usuario para iniciar el receptor y motor con el escritorio, sin VPN.
- Si vuelve a ocurrir un cierre, aparece «Copiar informe del último error»: clases
  de excepción y ubicaciones de código, sin mensajes, rutas privadas ni tokens.

## Cómo actualizar

1. Actualizar el receptor de escritorio y los servicios desde las fuentes 0.4.1.
2. Instalar [APK actual](../downloads/UmbrielConnect-0.8.0.apk) encima
   de la app anterior. **No desinstalar ni borrar datos.** Conserva paquete y firmante.
3. Abrir la app, permitir notificaciones y presionar Conectar. Un registro antiguo
   puede necesitar una última huella para migrar la clave limitada a biometría.
4. Después, en LAN directa se conecta sin huella; fuera de ella se ofrece el bloqueo
   del teléfono. Los permisos de plugins y accesibilidad siguen siendo opcionales.

Si la huella antigua ya no funciona, se necesita generar un registro nuevo desde la
computadora. Un PIN no puede cambiar retroactivamente la política de una clave antigua.
No se invalidan ni se copian silenciosamente identidades para evitar ese paso.

Los permisos remotos persistentes de 0.4.0 se retiran. La contraseña es la credencial
segura de Android; no hay que inventar otra ni entregársela a Umbriel o la computadora.

## Evidencia y límites

Se corrigió una incompatibilidad concreta del servicio, pero la captura del cierre
no contiene el stack del Galaxy. Las pruebas automatizadas y la firma del APK pasan;
falta confirmar el diálogo y la reconexión en el Samsung real. Android puede limitar
el segundo plano por batería o después de forzar la detención de la app.

[Seguridad y suposiciones de LAN](../SECURITY.md) · [Protocolo](REMOTE-SECURITY.md).
