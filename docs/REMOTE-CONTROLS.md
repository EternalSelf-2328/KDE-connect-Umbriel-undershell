# Controles remotos, fondos y límites — 0.2.0

## Qué permite la red y qué implementa la aplicación

Tailscale transporta el tráfico cifrado entre ambos equipos; no implementa un
mouse, un escritorio remoto o una galería por sí mismo. La computadora debe estar
encendida y el cliente VPN/receptor en ejecución. No se configuró arranque
automático ni encendido remoto de una computadora apagada.

En este equipo se usa userspace-networking, sin TUN ni DNS/rutas aceptadas. Solo
se publica dentro de la tailnet el TCP 31594 del receptor. Los demás plugins de KDE
Connect no quedan accesibles automáticamente fuera de la LAN por este montaje;
el descubrimiento local y sus permisos siguen separados. Los enlaces privados
MagicDNS deben abrirse desde el teléfono conectado a Tailscale, no desde un
navegador de computadora al que no se dieron esas rutas/DNS.

Una conexión directa suele responder mejor que una mediante relay DERP. Las rutas
con relay mantienen cifrado de extremo a extremo, pero pueden aumentar latencia
y reducir transferencia. [Tipos de conexión oficiales](https://tailscale.com/docs/reference/connection-types).

El límite de diez minutos, la huella por sesión y el cierre al bloquear o salir de
la app son decisiones de Umbriel Remote. No son límites de Tailscale. Se conservan
registro de un uso, certificado fijado, clave por uso y secuencia de mensajes. No
hay Key Attestation remota ni auditoría externa del intercambio experimental.

## Navegación y sonido

La app móvil presenta Navegación, Mouse, Sonido y apariencia, y Fondos. Al aprobar
la sesión se oculta la configuración de conexión y queda un encabezado compacto.

Navegación agrupa foco en cuatro direcciones, vista general y escritorios anterior
/siguiente. Pantallas y sesión conserva los controles avanzados; apagar pantallas
y bloquear requieren confirmación. Los widgets y perfiles de undershell quedan
bajo sus propios controles. En escritorio, Mi escritorio tiene Navegación, Sonido
y apariencia y Widgets; los perfiles guardados siguen disponibles.

Sonido lee el volumen con wpctl y lo modifica por el IPC de Noctalia. El deslizador
envía al soltar, admite 0–100 % y no modifica el volumen al consultar. Hay silencio,
anterior/reproducir-pausar/siguiente, tema claro/oscuro, no molestar y reposo. Los
cambios de reproducción usan el reproductor MPRIS activo elegido por Noctalia.

## Mouse de la computadora desde Android

El receptor C++ usa libXtst/X11 opcionales. En Umbriel, Xwayland puede mediar la
entrada mediante EIS y un aviso de permiso local. La sesión actual no ofrece el
portal RemoteDesktop: no se anuncia ese portal como funcional. La aprobación del
compositor, cuando se solicita, debe realizarse en la computadora. No se elude con
uinput, acceso a dispositivos, sudo o un permiso de accesibilidad del teléfono.

[Documentación local de Umbriel sobre entrada X11](https://docs.noctalia.dev/umbriel/security/).
El comportamiento depende de la versión de Umbriel realmente en ejecución y de
Xwayland con libei. No se reinició el compositor para probarlo en esta sesión.

En Android: activar Mouse, deslizar un dedo, tocar para clic izquierdo, tocar con
dos dedos para clic derecho, deslizar dos dedos para rueda y mantener para arrastrar.
El deslizador ajusta sensibilidad. Se acumula movimiento hasta 20 solicitudes/s y
solo se mantiene una solicitud de movimiento pendiente para evitar una cola por
latencia. No se sondea el touchpad en reposo.

El servidor valida números finitos, desplazamientos relativos de hasta 300 unidades,
rueda de hasta 3 pasos y botones izquierdo/central/derecho. Solo un cliente puede
ser dueño del mouse. Una activación cancelada no puede completarse después. Se
liberan botones al detener, desconectar, perder permiso, vencer o recibir bloqueo.
Se comprueba Noctalia desbloqueado antes de activar y se vigila ScreenSaver y su dueño.

**Prueba física pendiente:** mover el puntero real, confirmar el aviso de Umbriel,
arrastrar, desplazamiento, cambio de red y cierre al bloquear. Las pruebas usan
XTest simulado; no se movió el mouse del usuario como prueba.

## Integración con undershell-wall

Se utiliza undershell-wallctl list --json y apply, sin editar su base de datos,
configuración ni código privado. La biblioteca real consultada tenía 462 registros.
Se omiten los ocultos y se ofrecen búsqueda y favoritos. El daemon conserva el
comportamiento de salidas, renderizadores, hooks, paleta y profundidad que ya tenga
configurado. En esta versión se aplica a todas las pantallas (el destino predeterminado del
CLI); el selector por monitor queda pendiente. Aplicar puede actualizar el fondo
y el tema; siempre requiere elegir
y confirmar. Explorar nunca aplica un fondo.

La API remota entrega identificadores opacos, nombre, tipo y favorito; no rutas
absolutas. Al aplicar se resuelve el identificador contra la biblioteca leída del
proveedor y se invoca argv, sin shell. No acepta una ruta arbitraria del teléfono.

Las miniaturas solo se leen de archivos del usuario dentro del caché canónico de
undershell-wall. Se rechazan rutas externas, formatos no admitidos e imágenes fuera
de límites. Entradas PNG/JPEG/WebP de hasta 16 MiB / 8192 px; decodificación Qt limitada
 a 32 MiB; respuesta JPEG de hasta 320×200 y 96 KiB, calidad 72. La escala es un límite de
miniatura, no una resolución asumida de la pantalla. Doce elementos por página,
como máximo 10000 entradas y caché de metadatos en memoria durante 30 s. WebP se decodifica directamente con libwebp cuando está disponible, sin depender
del plugin de Qt. No se envían
los originales ni se reproducen fondos animados en el teléfono.

**Prueba física pendiente:** miniaturas y diálogo ampliado en el Galaxy y efecto
real de aplicar un fondo. La lectura del proveedor se verificó; la aplicación y sus
argumentos se probaron con un ejecutable falso. No se cambió el fondo real para probar.

## Lo que sigue fuera de estas interfaces

- Subir fondos desde el teléfono, enviar originales y selección por monitor; se usa
  la biblioteca existente y se aplica en todas las pantallas.
- Video continuo del escritorio o del celular; el touchpad no incluye imagen del escritorio.
- Teclado general de computadora desde este canal; el KDE Connect oficial conserva su plugin.
- Control/capturas del celular en el APK mínimo. La variante phone-control permanece
  en código, con accesibilidad y revisión de distribución pendientes.
- Cámara/webcam, integración scrcpy, audio Bluetooth y monitor virtual del original
  en la ventana C++. Siguen en la ruta heredada y no se declaran compatibles al 100%.
- Lectores nativos completos de notificaciones, SMS y contactos; se conservan motor
  y lanzadores oficiales donde existen, con sus permisos propios.
- Prueba completa SFTP, Bluetooth, portapapeles, llamadas, activación del widget,
  suspensión/reconexión y funcionamiento desde otra red con datos móviles.

No se migraron las aproximadamente 14000 líneas Python originales por completo.
La aplicación de escritorio, el receptor y las integraciones nuevas son C++20/Qt6;
el cliente Android es Java para los API nativos de Android y Keystore.

## Actualizar y probar

Instalar el APK mínimo 0.2.0 conserva paquete, firma y datos de 0.1.1. No borrar los
datos de la app ni regenerar la identidad TLS para una actualización normal. Cerrar
y volver a abrir la ventana de escritorio carga la nueva versión. Reiniciar solo
el receptor para habilitar los nuevos mensajes; conservar Tailscale conectado.

```bash
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
bash android/build.sh desktop
bash android/build.sh phone-control
python3 tests/android-artifacts.py
bash tests/android-guard.sh
```

Las pruebas de servicios usan programas y cachés temporales. Las de mouse enlazan
funciones XTest simuladas y se ejecutan en D-Bus privado. Los renders publicados
son offscreen con datos e imágenes sintéticos, sin capturas privadas del escritorio.
