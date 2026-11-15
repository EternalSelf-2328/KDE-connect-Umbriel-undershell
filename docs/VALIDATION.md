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
