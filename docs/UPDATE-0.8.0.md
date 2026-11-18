# Umbriel Connect 0.8.0 experimental

Actualiza el APK sobre la instalación anterior: conserva paquete, firmante y vínculos.
No necesitas borrar datos ni volver a importar un registro. Las funciones nuevas del
canal propio necesitan también el receptor de escritorio0.8.0.

## Archivos y personalización

**Archivos → Elegir archivos…** permite seleccionar hasta256 documentos en Android.
Se envían en una cola, uno después de otro, con progreso y confirmación SHA256 de cada
archivo. Cancelar detiene el envío y descarta la cola; los documentos pendientes
siguen seleccionados. Si termina la autorización, no se inicia el siguiente archivo.
En la PC el selector también admite varios archivos; Yazi conserva su cola propia.
Esto no significa transferencias paralelas. Fuera de LAN sigue existiendo la
confirmación de cada descarga en el teléfono.

En **Ajustes → Apariencia de la app** puedes elegir colores del sistema, verde agua,
lavanda, rosa o ámbar, además del tema del sistema, claro u oscuro. Cambian los fondos,
superficies, textos y acentos; no solo un botón. La selección se guarda. Cambiar
colores o idioma conserva la conexión y la selección de archivos.

La misma sección permite **Sistema / Español / English**. El catálogo inglés de la
interfaz propia se comparte con el escritorio; sus ajustes de idioma están en
**Ajustes → Idioma**. Vuelve a abrir las ventanas de escritorio para aplicar el cambio.
Los títulos de canciones, nombres de archivos y contenido personal no se traducen.

## Texto, teclado y portapapeles

**Inicio → Texto y teclado** permite escribir texto y enviarlo al campo enfocado en
la PC, además de Intro, Tab, retroceso y navegación. Es envío de texto por pulsación;
no transmite cada cambio del teclado virtual ni ejecuta el texto como una orden.
La entrada requiere una sesión gráfica desbloqueada y Xwayland/XTest con libfakekey;
Umbriel puede mostrar su propia autorización local de entrada.

El portapapeles tiene botones separados en ambos sentidos. Comparte **texto de hasta
64KiB**, solo al solicitarlo, sin vigilancia automática. La PC usa `wl-clipboard`;
no se coloca el texto en argumentos de procesos. En **Teléfono** de la PC también
puedes enviar texto o teclas al teléfono y compartir su portapapeles. El panel es
desplazable para conservar el acceso a los controles en ventanas pequeñas.

El control del teléfono requiere habilitarlo expresamente, sesión vigente y teléfono
desbloqueado. Para leer o escribir su portapapeles, Umbriel Connect debe estar abierto
en primer plano: se respeta la restricción de Android. El teclado hacia Android
requiere además su permiso de accesibilidad y un campo editable que no sea una
contraseña. Inserta en la selección actual y ofrece las teclas admitidas; no garantiza
que todas las aplicaciones Android permitan esas acciones.

## Abrir aplicaciones y controlar la sesión

**Inicio → Aplicaciones** muestra un buscador de programas instalados. Solo se pueden
abrir las entradas `.desktop` que anuncia el sistema; el teléfono envía su identificador,
no una línea de shell. Una apertura aceptada no garantiza que el programa permanezca
abierto ni que no falle por sus propios motivos.

Debajo están Bloquear, Suspender, Hibernar, Reiniciar y Apagar. Cada acción exige
confirmación, ligada a una autorización de un solo uso que caduca en30segundos.
Se usan las políticas locales de logind, sin `sudo`, cambios de polkit ni ejecución
de órdenes arbitrarias. Una política local o falta de soporte de hibernación puede
rechazar la solicitud. Suspender/apagar interrumpe el acceso remoto; despertar la PC
por Internet no forma parte de esta función.

## Ver la pantalla de la PC

**Inicio → Pantalla → Solicitar pantalla** abre una ventana de autorización en la PC.
Allí se elige cuál monitor compartir y se confirma. La solicitud por sí sola no envía
imágenes. La ventana permanece visible y **cerrarla o pulsar Detener revoca el permiso**.
También termina al bloquear la PC, desconectar, revocar el vínculo o salir de la vista
en el teléfono. Cada autorización dura como máximo cinco minutos.

La vista empieza en modo manual. «Actualización continua» solicita hasta **dos
imágenes por segundo**, con miniaturas JPEG de hasta1280píxeles por lado y300000bytes.
Puedes ampliar y usar dos dedos para hacer zoom; esos gestos no controlan el cursor.
No es transmisión de video de30FPS. La compresión reduce consumo de red y GPU, aunque
una red remota lenta puede añadir espera. El límite de imagen equivale, como máximo
teórico, a unos800kB/s tras Base64 a2imágenes/s; no es una medición de consumo real. Se capturan solo las imágenes solicitadas;
no hay un capturador consultando la pantalla en reposo.

La captura nativa usa los protocolos Wayland `ext-image-copy-capture-v1` y
`ext-image-capture-source-v1`; requiere que el compositor los ofrezca. No necesita
ADB, una cuenta externa ni abrir puertos nuevos. Viaja por la misma conexión TLS
local o proxy privado de Tailscale. En Android esta vista se protege de capturas y
recientes mediante `FLAG_SECURE`; esto no sustituye la confianza en el teléfono.

## Verificación y límites

Pasaron 61 pruebas Android del fork, 101 comprobaciones Java de guardas, 14 grupos
C++/TLS y 7 del motor privado. Las pruebas ejercitan colas con dos streams, guardas de sesión, ambos sentidos del
portapapeles, confirmación de energía, paletas legibles y navegación en inglés. La
captura se probó con un compositor headless aislado y contenido sintético, incluida
la revocación al cerrar el socket de la ventana de autorización. Las pruebas de
energía llaman a un logind falso; no suspenden ni reinician la computadora real.

Se conservan las regresiones contra el borrado de canales de notificación y el acceso
a campos privados de Android, comprobadas también en el APK optimizado y firmado.
Consulta los resultados reproducibles y dependencias en [Compilación](BUILDING.md).
La instalación y validación física de esta versión en el Galaxy siguen pendientes;
no se presentan como probadas las políticas de Samsung ni todas las aplicaciones
reales. No se ha leído el portapapeles, escrito texto ni capturado la pantalla privada
del escritorio para estas comprobaciones.
