# Validación — 2026-11-15

## Entorno observado

| Componente | Versión |
|---|---|
| KDE Connect | 26.08.3-2.1 |
| Qt6 Core / DBus / Widgets / Network | 6.12.0 |
| Umbriel en disco | 0.1.0, 88ba829c6152 |
| Noctalia en disco | 5.5.0, 12d96bd9d9c1 |
| undershell | 1.8.0 |
| CMake | 4.5.0 |
| Compilador | GCC 16.2.1 |

La sesión activa de Umbriel rechazó `config-get`, aunque el binario en disco lo
ofrece. Por eso la ruta nueva usa acciones `msg` y no depende de ese método. No se
reinició la sesión remota para actualizar el compositor en marcha.

## Pruebas automatizadas

- Suite QtTest nativa: **15 resultados aprobados** (13 casos funcionales más
  inicialización y cierre), sin fallos ni omisiones.
- Teléfono falso exportado en un bus D-Bus privado: propiedades tipadas, batería,
  capacidades, confianza, conexión, aceptación, archivos con espacios y caracteres
  especiales, errores del servicio y debounce de señales.
- CLI real como subproceso contra ese teléfono falso: `list`, `watch`, estado
  inicial, cambio por evento y códigos de salida.
- Servidor temporal de undershell: solicitud exacta, respuesta parcial, error
  semántico, éxito, timeout, rechazo de saltos de línea y argumentos inválidos.
- Configuración temporal: preservar órdenes y otras claves, respaldo byte a byte,
  publicación idempotente, retirada, rechazo de JSON corrupto y rutas inválidas.
  Incluye 20 publicaciones/retiradas rápidas: se detectó y corrigió una colisión
  de nombres de respaldo dentro del mismo milisegundo, añadiendo un UUID.
- Plugin: **23 comprobaciones** con los archivos Luau reales en una VM de Luau:
  estado inicial, conectado/desconectado, filtrado por confianza, batería desconocida,
  argv seguro, escape de ruta, estado al terminar el watcher y mensajes inválidos.
- `noctalia plugins lint`: **0 errores y 0 advertencias**.
- Entrada de escritorio validada con `desktop-file-validate`.
- Ventana renderizada con `QT_QPA_PLATFORM=offscreen`, usando exclusivamente datos
  sintéticos. No se capturaron pantallas del usuario.

El sandbox bloquea la creación de sockets; la suite nativa se ejecutó con permiso
fuera del sandbox, usando un bus privado y archivos temporales. No se alteraron
configuraciones ni servicios reales durante las pruebas.

## Comprobaciones reales de solo lectura

El cliente nativo consultó con éxito el KDE Connect real: servicio disponible,
un dispositivo emparejado y desconectado, sin datos parciales. Leyó **18 perfiles**
del undershell real y generó **57 órdenes** de vista previa (39 estáticas y 18
perfiles). No se publicaron órdenes en el dispositivo real.

## Mediciones locales

Con compilación `RelWithDebInfo`, una consulta real `list` desde proceso nuevo tardó
**24,46 ms**. El watcher, después del estado inicial, tenía **31.128 KiB RSS**
(aproximadamente 30,4 MiB). Durante una ventana de **3 segundos** en reposo acumuló
**0 ticks de CPU**, con resolución de 100 ticks/s. Es una muestra corta, no una
promesa de CPU cero en todas las situaciones; no incluye transferencias ni la
memoria del daemon oficial.

El ejecutable de compilación medido tenía 7.149.608 bytes con información de
símbolos. La instalación elimina símbolos con `cmake --install build --strip`;
la cifra del binario instalado se registra en la documentación local del sistema.
No se midió una versión Python equivalente y no se afirma una mejora porcentual.

## Pendientes de hardware y sesión

El usuario estaba fuera de casa usando Codex remotamente. No hubo teléfono físico
disponible para verificar tráfico Android, Bluetooth, notificaciones, portapapeles,
MPRIS, llamadas, entrada remota o SFTP. Tampoco se activó el widget en las barras,
se apagaron pantallas, se bloqueó la sesión ni se cambiaron widgets reales.

La [matriz de compatibilidad](COMPATIBILITY.md) separa lo implementado, lo probado
y las funciones del cliente original que aún no están migradas a la ventana C++.

## Ampliación biométrica del mismo día

Se compiló e instaló `umbriel-connect-remote` y el helper `umbriel-remote-session`.
APK Android API 36 empaquetado y firmado con APK Signature Scheme v3; firma verificada.
La primera compilación de esta fase ocupaba 33.241 bytes (APK) y 333.848 bytes
(receptor); las cifras posteriores al rediseño se registran abajo.
No hay mediciones de CPU/RAM en el Galaxy A24 físico.

Suite previa al rediseño: 3/3 grupos de CTest aprobados (12 casos TLS, pruebas del helper y la
suite QtTest de 15 resultados). La misma SessionGuard Java de la app pasó 20
comprobaciones. Se conservaron 23 checks Luau y lint0/0. Las pruebas usan un
receptor loopback, claves y archivos temporales; no capturaron pantallas reales.

Se probó recuperación tras interrupción del receptor y limpieza de sus procesos
hijos. El código Android impide aceptar una autorización enviada antes de firmar
con la huella o utilizar un segundo mensaje de autorización para prolongar la lease.
Tailscale 1.104.7 se preparó sin sudo y se inició en userspace, sin TUN, DNS/rutas
aceptadas, SSH ni exit node. La cuenta sigue pendiente de inicio de sesión del
usuario; no se inició el receptor de control ni se generó un registro real del teléfono.

Pendiente: primer arranque y huella real del Galaxy A24 con Android 16/One UI 8.5,
notificaciones, accesibilidad, capturas y gestos, bloqueo, rotación y conexión
Android↔computadora sobre la tailnet. La implementación es experimental y el
intercambio de aplicación no tiene auditoría externa ni Key Attestation remota.

## Rediseño de escritorio y Android

Suite final **4/4 grupos de CTest aprobados en 8,90 s**, incluyendo 7 resultados del
nuevo QtTest de diseño (5 casos más inicialización/cierre). Incluye todas las
pestañas a 760×480, 780×600, 900×640 y 1100×720 sin scroll horizontal, IJKL/flechas,
escritura de letras en campos, contraste >= 4,5:1 y recarga atómica de Noctalia.
Renders offscreen claros, oscuros y monocromos revisados con datos sintéticos.
El [informe de diseño](DESIGN.md) conserva las capturas y los límites de inspección.

APK actualizado: **37.337 bytes**, firma v3 verificada. La SessionGuard de Android
mantiene sus 20 comprobaciones aprobadas. No se renderizó Android en emulador ni en
el teléfono. Ninguna captura publicada representa una conexión física validada.

Instalación nativa final: `366744`  bytes; receptor
`379544`  bytes, sin símbolos. No se repitieron las mediciones de
latencia/RSS iniciales ni se extrapolan al nuevo diseño.

## Bloqueo físico de Play Protect y APK mínimo 0.1.1

El primer intento de instalación real del APK 0.1.0 fue bloqueado por Play Protect
(según captura enviada por el usuario). No se llegó a probar la app o la huella.
El [diagnóstico](PLAY-PROTECT.md) documenta la causa probable y las limitaciones.

Se construyeron dos variantes aisladas. La descarga mínima 0.1.1 excluye del
manifiesto, recursos y DEX el servicio de accesibilidad/captura/gestos. Se verifican
sus permisos exactos, firma v3 y la misma identidad que0.1.0. Una prueba Java ejecuta
el backend mínimo y confirma su rechazo; SessionGuard mantiene 20 checks aprobados.
El control inverso queda en una variante de código opcional no publicada como APK.

APK publicado: **33170 bytes**, SHA256
`6b6f700cd347248f3a5641a375413d0662bfcb8c99888b40de969be4df21a064`.

No se afirma aprobación de Google ni instalación exitosa de 0.1.1 en el Galaxy.
No se cambió código nativo, la red, la sesión remota ni las protecciones del teléfono.

## Ampliación 0.2.0 —2026-11-16

Seis grupos de CTest aprobaron en 9,98 s: 13 pruebas TLS reales con clientes simulados,
15 resultados nativos, 7 diseño, 6 servicios y 5 mouse (QtTest incluye init/cleanup),
más el helper. Los nuevos casos comprueban autenticación, rutas privadas, paginación,
favoritos, miniaturas JPEG, argumentos de aplicación, volumen y liberación de botones.
XTest está simulado; no se tocó el puntero o el fondo real. El bus de pruebas es privado.

APK mínimo 49554 bytes, variante opcional 53721  bytes; firma v3 y misma identidad,
versionCode 3. La inspección del manifiesto/DEX confirma permisos mínimos, inclusión
de RemoteDashboard/Touchpad y exclusión de accesibilidad en la descarga. SessionGuard
mantiene 20 checks. No hay render físico/emulador Android de la nueva interfaz.

Lecturas reales: 462 registros en undershell-wall, Noctalia desbloqueado y portal
RemoteDesktop ausente. No se publicaron sus imágenes. Renders Qt offscreen con
imágenes sintéticas, temas claros/oscuros y tamaños compactos comprobados.

Las pruebas reales de mouse, permisos del compositor, cambio de fondo desde el
celular y otra red siguen pendientes. [Detalle de alcance](REMOTE-CONTROLS.md).

Se detectó ausencia del plugin Qt WebP en el equipo y se añadió decodificación
directa con libwebp 1.6.0 ya instalado. Un caso adicional comprueba WebP→JPEG
escalado sin ese plugin; la suite de servicios ahora tiene 6 resultados.

Comprobación real sin aplicar fondos: 462 registros visibles,
miniatura JPEG de 320×180 y 12137 bytes,
obtenida por el mismo adaptador C++. Solo se guardaron métricas, ninguna imagen real.
Después del ajuste final de textos se repitió diseño: 1/1 grupo, 7 resultados aprobados.

APK mínimo final SHA256 `0513119426ab7b7b8b4c0bf18eff8c3188227203e7c6f43413d44e23be7b3ae2`.
Escritorio instalado: 478104 bytes;
receptor: 470264 bytes, sin símbolos.
Claves y registro conservados, receptor reiniciado, Tailscale sin reiniciar.

## Red local, archivos y latencia — 0.3.0 / 2026-11-16

Las siete suites aprobaron en **19,21 s**: 18 casos TLS, helper, 6 resultados de
transferencias, 7 servicios, 5 pointer, 15 nativos y 7 diseño (QtTest incluye
inicialización/cierre). Los casos nuevos ejercitan listado/CLI propios, eventos de
conexión, LAN y certificado, archivo de 12 288 000 bytes con SHA256, rechazo,
desconexión, recepción desde teléfono, nombres/rutas/tamaños/offsets y filtros de
origen/carpeta/tipo. La prueba de eventos procesa tramas JSON por líneas: varios
avisos pueden llegar juntos en una lectura TCP. GIO se mantiene local en las pruebas.

Android: 20 checks SessionGuard + 9 del búfer de movimiento. APK mínimo de 57 746
bytes, versionCode 4 y misma identidad de firma. Firma v3, manifiestos, separación
DEX y seis permisos verificados en ambas variantes; control del teléfono excluido
de la descarga. Variante opcional compilada, no publicada. Nueva UI Android y
MediaStore en Samsung pendientes de prueba física.

Yazi: ejecución real en PTY aislado, emisor falso. Ctrl+K y opener verificados con
uno/varios archivos, espacios, apóstrofo y metacaracteres. No se enviaron archivos
personales durante la prueba. Se conservó un respaldo de ambos TOML y se validó
que los demás atajos/openers conservan su contenido.

Adaptador C++ real: **276 fondos locales en wallpapers, 186 de Wallpaper Engine**;
miniatura JPEG real de 320×180, 12137 bytes, sin guardar/publicar la imagen ni
aplicar un fondo. Receptor instalado con un listener LAN y descubrimiento activos;
TLS loopback/LAN con el mismo certificado. Tailscale siguió Running, sin cambios
de rutas, DNS o firewall. Registro y claves conservados al instalar.

Usuario confirmó que el mouse 0.2.0 funciona pero se siente lento. El cambio a
agrupación de 16 ms, envío sin esperar respuesta y TCP_NODELAY se comprueba en código
y pruebas de búfer; no se afirma una nueva latencia física ni se movió el puntero
real para medir. Renders Qt de filtros y selector de envío revisados con datos
sintéticos; APK 0.3.0 aún requiere actualizarse en el Galaxy.

## Archivos grandes — 0.3.1 / 2026-11-16

Investigación del código oficial de KDE y decisiones: [LARGE-FILES.md](LARGE-FILES.md).
Se eliminó el tope propio de1 GiB; se comprueba espacio libre y se utiliza una
ventana de ocho bloques en ambos sentidos. Se conserva la expiración de controles:
solo un archivo previamente aprobado puede terminar, sin permitir órdenes o archivos
nuevos. Las pruebas también rechazan ofertas pendientes aceptadas demasiado tarde.

Suite final7/7 en23,35s:21TLS,helper,7resultados de transferencias,7servicios,
5pointer,15nativo,7diseño (incluyen init/cleanup QtTest). Android:20SessionGuard,
9MotionBuffer y23TransferScope; total52 comprobaciones Java. Dos variantes APK
firmadas; descarga mínima versionCode5,61842bytes y mismos seis permisos, sin
accesibilidad. Se comprueba también la presencia de TransferScope en el DEX.

Pruebas grandes opcionales reales contra el receptor TLS C++, cliente simulado:

| Tamaño por sentido | PC→cliente simulado | Cliente simulado→PC | Mayor VmRSS muestreado del receptor |
|---|---:|---:|---:|
|1 GiB +17bytes|29,89s|25,27s|41176KiB|
|4 GiB +17bytes|77,15s|100,68s|41116KiB|

Se verificó SHA256 en ambos sentidos y se releyó el archivo recibido desde disco.
Se usaron archivos sintéticos temporales (origen disperso), búferes acotados y una
lease de control acortada a un segundo: la transferencia terminó con controles
vencidos. Los temporales se eliminaron al concluir. El RSS es una muestra del
receptor, no el consumo total del sistema, caché del kernel o cliente Android.
No extrapolar estas velocidades loopback al Wi-Fi, Tailscale ni al Galaxy.

Sin verificar físicamente: archivos grandes y espacio/MediaStore del Samsung,
progreso/evitar apagado automático y vista «Solo transferencia». Cerrar la app o
bloquear manualmente sigue cancelando la transferencia. No se añade WAKE_LOCK ni
recepción permanente en segundo plano. No se aplicaron fondos ni movió el puntero.


## Diseño de selector — 0.3.2 / 2026-11-16

Compilación C++ completada sin advertencias nuevas y CTest **7/7 en 22,72 s**.
Diseño: **11 resultados QtTest** incluyendo inicialización/cierre. Se verifican
selección y conservación del destino, exclusión de conexiones KDE/no autorizadas,
campos con metacaracteres sin shell, estados sin receptor/sesión, ventanas compactas
y recepción/error/reintento de varios archivos contra un QLocalServer falso. El
reintento envía A,B,B, sin repetir A. Se evita reactivar Enviar tras completar.

Se corrigió un desbordamiento horizontal con nombres largos detectado en 400×440.
Renders offscreen de claro, oscuro y paleta Noctalia real revisados visualmente con
archivos y teléfono sintéticos. La ventana principal oculta controles de otro
transporte al seleccionar Umbriel Remote. Cierre de sockets locales protegido
durante destrucción del cliente. No hubo cambios en protocolo, permisos ni APK. Tras ampliar el ajuste de textos
largos a estados de envío/error, diseño se repitió: 1/1 en 1,75 s.

Instalado escritorio 0.3.2 con respaldo previo. No se reiniciaron VPN, compositor
ni receptor activo. Se abrió el selector propio para revisar el APK existente,
sin pulsar Enviar. La ventana blanca observada era `kdeconnect-handler`; la
instancia Yazi activa se inició antes del cambio de su configuración. Esta ya
apunta al ejecutable propio y requiere reabrir Yazi. No se enviaron archivos
personales ni capturó el escritorio durante las comprobaciones.


## Inicio y registro — 0.5.0 / 2026-11-17

31 pruebas Android aprobadas: HomeNavigation4, RegistrationUi4, AuthenticatedUi4,
ConnectionRegression10, IncomingShare8 y HomeRender1. Renders de Views reales con
runtime gráfico nativo/API 36 y datos sintéticos: claro/oscuro,320/412/840 dp, texto 130%,
conectado/buscando/remoto/primer registro, Mouse, Apariencia, Archivos y Ajustes.
Se revisaron los PNG; controles multimedia usan iconos para evitar texto partido,
barra inferior persistente y contenido desplazable. [Renders y contraste](DESIGN-ANDROID-HOME.md).

10/10 suites nativas aprobadas en 27.52 s, incluidas27 pruebas TLS y la inicialización
idempotente de la identidad al primer inicio. El QR se generó y decodificó con
Prison/PrisonScanner, con datos sintéticos; exportación privada0600 verificada.
Se repitió solo diseño tras ajustar dos etiquetas del escritorio.101 comprobaciones
Java existentes de guardas, entrada y transferencias aprobadas; variantes mínimas
compiladas/firmadas e inspeccionadas, sin sustituir la descarga histórica mínima.

APK completo0.5.0:11 531 919 bytes, SHA256
`cd7df3c27c4cdeb5245da9e5c0627939901455000c2d1be8a96756bb1320f122`,
versionCode 9/min 30 / target 37, mismo paquete/certificado, firma v3 y no debuggable.
Inspección del manifiesto fusionado, deep link de registro y clases propias en DEX;
bytecode R8 de LocalNetworks conserva getAddress/getPrefixLength públicos. El APK
suma32 768 bytes frente a0.4.2. Mapas R8 conservados solo en almacenamiento privado.

Escritorio0.5.0 instalado con respaldo0.4.2. Ambos servicios propios activos/habilitados;
receptor reiniciado tras comprobar cero sesiones. TLS1.3 local coincide con certificado
almacenado; identidad conservada. Daemon oficial sin propietario D-Bus. Sin cambios a
VPN/rutas/DNS/firewall/compositor, sin mover puntero/aplicar fondos y sin capturas privadas.
No había Galaxy conectado por ADB: huella/PIN, teclado Samsung, cámara que abre URI y
funciones físicas se mantienen pendientes. No se afirma aprobación de Play Protect.


## Vinculación local y escritorio —0.6.0 / 2026-11-17

33pruebas Android únicas aprobadas (31de UI/conexión/URI/render +2de comparación de
claves/TLS/nonce y rechazo de descubrimiento ajeno/público). 31casos TLS reales en
directorios temporales: consentimiento PC requerido, código ligado a claves/TLS,
rechazo/cancelación sin confianza y revocación de grupo conservando otro dispositivo.
Diez suites nativas verificadas: ocho pasaron en primera ejecución; seguridad y
diseño se corrigieron/repetieron2/2 en28.80s y Qt se repitió1/1 tras las últimas etiquetas.
El archivo clients.json es vacío tras init: la prueba exige contenido vacío antes
de aprobar.101checks Java y variantes mínimas también aprobados.

Renders Qt sintéticos claros/oscuros1100×720 y compacto760×480, sin captura privada;
sidebar corregida tras ver filas solapadas. Contraste texto/surface13.84/14.87,
muted8.80/6.57 y acción7.22/6.24, con script canónico y paletas Qt.
APK versionCode10/min30/target37, 11531919bytes, SHA256
`19d2823f4a1a4802fe9c562d22c65c7af07d92a9d035a493745b385da1bdfb1a`, mismo firmante, no debuggable, manifiesto/DEX/bytecode R8
y checksum verificados. Mapas conservados privadamente; no se incluye ningún registro
real ni clave de usuario. Escritorio instalado, servicios activos/habilitados,
TLS1.3/pin almacenado y respuesta UDP nueva comprobados; cero sesiones al reiniciar.
No se modificó red/VPN/DNS/firewall/compositor. Samsung físico pendiente.
