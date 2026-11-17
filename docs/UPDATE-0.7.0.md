# 0.7.0 — reproducción, salidas de audio y touchpad

2026-11-17 · escritorio/receptor 0.7.0 · Android versionCode 12 · experimental.

Instala [UmbrielConnect-0.7.0.apk](../downloads/UmbrielConnect-0.7.0.apk) sobre la aplicación
actual. Conserva paquete, firma, datos y vínculo. No hace falta desinstalar ni borrar datos.
Actualiza también el receptor de escritorio con `./install.sh`: los nuevos controles
multimedia necesitan 0.7.0 en ambos extremos. Si ya tienes una GUI anterior abierta,
ciérrala y vuelve a abrirla. El instalador conserva los servicios en ejecución; reinicia
solo `umbriel-connect-remote.service` cuando no haya una transferencia en curso.

## Reproductor del teléfono

Inicio y Control → Sonido muestran título, artista, carátula, aplicación de origen y
«En reproducción»/«En pausa». El botón central indica la acción disponible: pausar
cuando está reproduciendo y reproducir cuando está en pausa. Espera el estado
confirmado por la PC; no supone que la orden funcionó.

Si hay varias aplicaciones, «Cambiar reproductor» permite elegir cuál controlar.
La selección queda guardada en el teléfono; Automático prioriza los reproductores
que están reproduciendo. Cada orden se dirige al mismo reproductor cuyos datos se
muestran, y respeta sus capacidades. El origen web, si está disponible, muestra solo
el dominio, sin rutas, parámetros ni credenciales.

El receptor consulta [MPRIS por D-Bus](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html)
y escucha los cambios publicados por las aplicaciones. No mantiene un temporizador
consultando el reproductor en reposo. Android se suscribe al mostrar el panel y deja
de hacerlo al salir o pasar al fondo. Las notificaciones propias solo se envían a
sesiones autenticadas, vigentes y suscritas. Las versiones anteriores no reciben
mensajes nuevos si no se suscriben.

Las carátulas se reducen a JPEG de hasta 256 × 256, con entrada y memoria acotadas.
Solo se admite la imagen anunciada por el reproductor, identificada por una clave
opaca; el teléfono no puede pedir rutas o URLs arbitrarias. Para HTTPS se comprueba
certificado, destino público y cada redirección; se rechazan red privada, loopback,
Tailscale, credenciales en la URL y puertos distintos de 443. No se envían cookies.

**Disponibilidad:** la aplicación de la PC debe publicar MPRIS. Algunos navegadores,
páginas y archivos no ofrecen título, artista o imagen. En esos casos aparece una
carátula neutra o «Sin título»; no se inventan datos ni se promete compatibilidad con
todas las páginas. Las imágenes HTTP o de servidores privados no se descargan.

## Salida de audio de la PC

«Salida» muestra el dispositivo predeterminado y abre la lista de salidas disponibles.
Elegir una establece el destino de los sonidos siguientes y solicita mover también
los flujos que ya están reproduciéndose. Si alguna aplicación rechaza el movimiento,
la interfaz informa del resultado parcial. No cambia perfiles de tarjetas ni activa
salidas que no estén disponibles en el servidor de audio.

Se usa la [API asíncrona de libpulse](https://www.freedesktop.org/software/pulseaudio/doxygen/async.html),
compatible con PulseAudio y PipeWire-Pulse, y sus
[eventos de cambio](https://www.freedesktop.org/software/pulseaudio/doxygen/subscribe.html).
La dependencia es opcional al compilar; si falta, se informa de que no hay selector.
El volumen/mute se lee del dispositivo predeterminado mediante esa misma conexión;
solo se usa la consulta anterior como alternativa cuando no está disponible.

## Mouse y diseño

En **Ajustes → Controles → Mouse**, o **Control → Mouse → Dirección del mouse…**:

- Invertir el desplazamiento con dos dedos.
- Invertir el eje horizontal del cursor.
- Invertir el eje vertical del cursor.

Son opciones independientes y persistentes. La sensibilidad conserva su ajuste.
Los cambios de dirección se aplican al cerrar el diálogo; se descartan movimientos
pendientes y se suelta cualquier arrastre para evitar saltos o botones retenidos.

El primer movimiento ya no espera los 16 ms fijos anteriores. Los siguientes se
agrupan con intervalo previsto de 8 ms, hasta 125 envíos/s, sin consultas en reposo.
Se conservan la cola acotada, TLS y TCP_NODELAY. Esto reduce la espera introducida
por la aplicación; **no constituye una medición de latencia del Galaxy ni elimina
la demora del Wi-Fi, un relay o transferencias que comparten la conexión TCP**.
En la misma LAN se usa la ruta directa sin necesidad de Tailscale.

El menú ampliado de Widgets queda separado 12 dp del botón. Ajustes usa un engranaje
cerrado dentro de un botón de imagen centrado y con foco visible, compartido con
la navegación inferior. Las pantallas se comprueban con Views Android y datos
sintéticos; no son capturas del teléfono ni de contenido personal.

## Comprobaciones

47 pruebas Android del fork aprobadas, 101 comprobaciones Java de guardas y 12
grupos C++/TLS aprobados. Incluyen selección explícita de reproductor/salida, estado
confirmado, persistencia de inversión y primer movimiento sin la espera de 16 ms.
Se comprobó también la descarga HTTPS de una imagen pública de prueba, conservando
verificación TLS y destino fijado; HTTP/1.1 mantiene el Host original al fijar la IP.
Consulta [las instrucciones de pruebas](BUILDING.md).
Las pruebas de audio levantan un servidor PipeWire y una política WirePlumber privados,
sin monitores de hardware, con dos salidas nulas y una reproducción sintética.
No cambian la salida ni la música de la sesión del usuario.

La validación física en el Galaxy, incluidos carátulas reales, cambio de salida y
latencia percibida, sigue pendiente. Se mantienen las correcciones de notificaciones
activas y de los accesos públicos de Android que causaron los cierres anteriores.
