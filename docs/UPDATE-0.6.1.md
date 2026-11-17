# Android 0.6.1 — canales de notificación y espacios

2026-11-17 · versionCode 11 · mismo paquete y firma · escritorio 0.6.0 compatible, sin cambios.

## Actualizar

Esta corrección está incluida en la [actualización 0.7.0](UPDATE-0.7.0.md).
Instala el APK actual sobre la versión
actual. Conserva datos, vínculos, claves y permisos; no requiere borrar datos ni
volver a registrar por esta corrección. La corrección 0.6.1 era compatible con el escritorio 0.6.0. Para las funciones nuevas
de 0.7.0, actualiza ambos siguiendo su guía.
La prueba de esta actualización en el Galaxy físico sigue pendiente.

## Cierre reportado al abrir opciones

El informe 0.6.0/API 36 termina en `NotificationManager.deleteNotificationChannel`,
invocado desde `NotificationHelper.initializeChannels` durante `Application.onCreate`.
El helper heredado eliminaba todos los canales que no estaban en su lista. El canal
`session` pertenece al servicio de conexión propio y no estaba en esa lista.
Android 16 impide borrar un canal asociado a un servicio foreground activo o trabajo
iniciado por el usuario y puede lanzar `SecurityException`.
[Comprobación de Android 16 en AOSP](https://android.googlesource.com/platform/frameworks/base/+/refs/heads/android16-release/services/core/java/com/android/server/notification/NotificationManagerService.java).

Se retira la limpieza global: el helper registra sus canales de forma idempotente
y conserva los canales de los demás módulos y servicios. No se detiene el servicio,
no se ocultan notificaciones ni se ignora una excepción de seguridad para continuar.
El cambio se aplica mediante el overlay del fork, sin modificar el submódulo fijado.

Se reprodujo la condición en una prueba API 36 con la notificación de sesión presente
y un gestor que rechaza borrados: la versión previa falló con `SecurityException`.
Con el parche, dos inicializaciones conservan la sesión, su notificación, el canal
de otro módulo y los diez canales del motor. El APK 0.6.0 publicado contiene la llamada
de borrado; la validación de 0.6.1 inspecciona el método release y exige que no exista.
No se presenta esta simulación como una prueba del framework Samsung real.

## Espaciado

- Pestañas separadas 8 dp y 16 dp entre barra y tarjetas. Solo se desplaza lo necesario
  para mantener visible la pestaña elegida; conserva navegación horizontal y teclado.
- 12 dp entre touchpad y botones, entre botones de una fila y entre grupos de acciones.
  El último botón se alinea con el margen interior de la tarjeta.
- Tarjetas con 20 dp interiores y 16 dp entre secciones. Botones desactivados mantienen
  su superficie y texto secundario, evitando etiquetas que parecen sueltas.
- Filtros de fondos con separaciones uniformes; miniaturas separadas del título,
  bordes recortados a su forma y rejilla adaptable a dos/tres columnas.
- El aviso «Mouse activado» se limpia al navegar a otra sección. La entrada se detiene
  al salir del touchpad, conservando la conexión autorizada.

[Renders de Views Android con datos sintéticos](screenshots/0.6.1/). Incluyen ancho 393 dp
con densidad 1.5, cercano a la geometría de las capturas aportadas, temas claro/oscuro,
texto 130% y pantallas 320/412/840 dp. Son recursos Android estándar/API 36; no emulan toda
la capa visual OneUI. No se copian los wallpapers o capturas privadas del usuario.

37 pruebas Android aprobadas, incluidas regresión de notificaciones, geometría del
espaciado, márgenes en varios tamaños y limpieza del mensaje del mouse;101 comprobaciones Java
de guardas existentes y variantes mínimas también comprobados. La versión release
mantiene las comprobaciones de R8/red pública, firma y manifiesto fusionado.
