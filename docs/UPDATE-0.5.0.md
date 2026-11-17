# 0.5.0 — Inicio y controles propios en Android

2026-11-17 · versionCode 9 · mismo paquete y certificado de publicación.

## Actualizar

Instala [UmbrielConnect-0.5.0.apk](../downloads/UmbrielConnect-0.5.0.apk) sobre la
versión actual. Conserva vínculos, preferencias y claves; no requiere desinstalar.
Cierra y vuelve a abrir la ventana de escritorio después de actualizar sus binarios.
La prueba física del APK en el Galaxy A24 sigue pendiente.

## Uso diario

- **Inicio:** estado de la computadora y seis accesos a Mouse, Navegación, Sonido,
  Apariencia, Fondos y Widgets; controles de reproducción y volumen.
- **Control:** el mismo dashboard, abierto directamente en la función elegida.
  Entrar en Mouse activa el touchpad; salir de esa página detiene la entrada sin
  cerrar la conexión. Cambiar de sección conserva una sesión válida.
- **Archivos:** enviar/recibir usando la conexión existente; estados y recuperación
  de errores sin pasar por la configuración de dispositivos.
- **Ajustes:** conexión, registros, dispositivos/plugins, permisos, pausa y diagnóstico.

La notificación y las entradas antiguas del motor abren esta interfaz propia. El
motor Kotlin/Java se conserva, pero su drawer ya no es la portada de la aplicación.
Las pantallas especializadas de algunos plugins todavía proceden del motor: no se
anuncia una reescritura visual de cada función ni una migración de Android a C++.

## Conexión sin pasos repetidos

Después de vincular inicialmente, Inicio busca la computadora en LAN y se conecta
sin diálogo de identidad si verifica certificado, ruta directa y clave autorizada.
Un Wi-Fi cualquiera no demuestra LAN: no se retira la verificación de seguridad.
Fuera de LAN aparece la acción de autorización, con huella o PIN/patrón/contraseña
mediante el sistema Android. No abre ese diálogo automáticamente ni renueva sesiones
remotas sin una nueva autorización. Siguen vigentes los límites de 0.4.1/0.4.2 y la
migración explícita de una clave biométrica antigua.

Si aún no existe registro, usa en el escritorio **Acceso protegido → Vincular
teléfono… → Generar vínculo de 5 minutos**. Escanea el QR desde la cámara del teléfono
para abrir Umbriel Connect, o guarda/importa el archivo privado desde Inicio. El
registro dura cinco minutos y se usa una vez. Siempre se confirma antes de guardarlo;
un nuevo certificado de computadora desactiva el permiso previo de controlar el
celular. No publiques el QR, enlace, JSON ni claves privadas. No se añade un permiso
de cámara a Umbriel para este paso. El receptor inicializa su identidad TLS al primer
inicio y la conserva en los siguientes, sin exigir ejecutar `init` manualmente.

La confianza del motor compatible es independiente: **Ajustes → Dispositivos y
funciones** permite compararla y configurar plugins. No se vincula automáticamente
por tener el mismo nombre/IP. En la misma LAN no necesitas Tailscale; fuera de ella
consulta sus [restricciones](COMPATIBILITY.md#alcance-de-red). Esta actualización no
inicia VPN ni cambia rutas, DNS, firewall o configuraciones de Umbriel/Noctalia.

## Diseño y comprobaciones

Interfaz nativa con paleta dinámica clara/oscura, superficies sólidas, iconos de una
familia, foco visible y navegación inferior persistente. Los controles cambian de
dos a tres columnas con el ancho; el contenido se desplaza sin ocultar la navegación.
No añade animaciones permanentes ni blur. [Renders y auditoría](DESIGN-ANDROID-HOME.md).

Se comprobaron31 pruebas Android, 10 suites nativas (incluidas27 pruebas TLS), lectura
real del QR y101 comprobaciones Java de las políticas existentes. Los renders usan
Views Android y fuentes nativas con datos sintéticos, no capturas de teléfonos reales.
La validación del APK final verifica manifiesto fusionado, versión, firma y checksum,
y desensambla el bytecode R8 para conservar la corrección del cierre de 0.4.2.
La prueba en Samsung y de todos los plugins/portales físicos continúa pendiente.
