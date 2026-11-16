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
APK Android API36 empaquetado y firmado con APK Signature Scheme v3; firma verificada.
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

Instalación nativa final: `366744` bytes; receptor
`379544` bytes, sin símbolos. No se repitieron las mediciones de
latencia/RSS iniciales ni se extrapolan al nuevo diseño.
