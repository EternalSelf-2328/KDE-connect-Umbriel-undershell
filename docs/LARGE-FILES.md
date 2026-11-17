# Archivos grandes: investigación de KDE Connect y cambio 0.3.1

## Qué se comprobó en KDE

Se revisaron fuentes oficiales de KDE el 2026-11-16. Las versiones de los repositorios
pueden cambiar; estos son los archivos inspeccionados, no una promesa para toda
versión/plataforma histórica.

- [NetworkPacket de escritorio](https://github.com/KDE/kdeconnect-kde/blob/642a710abd9dfff4358b5d943ca61965fb34ec6e/core/networkpacket.h)
  representa payloadSize con qint64 y conserva el contenido en un QIODevice.
- [UploadJob](https://github.com/KDE/kdeconnect-kde/blob/642a710abd9dfff4358b5d943ca61965fb34ec6e/core/backends/lan/uploadjob.cpp)
  lee bloques pequeños y continúa cuando el socket vacía los datos pendientes. No
  carga el archivo completo en RAM.
- [LanLink de Android](https://github.com/KDE/kdeconnect-android/blob/f753fddd2a62676ff0537fd28e191f0ec4c50d9e/src/main/java/org/kde/kdeconnect/backends/lan/LanLink.kt)
  anuncia un puerto para el contenido, abre otra conexión TLS, copia desde InputStream
  a OutputStream con un búfer de 256 KiB y cuenta el progreso con Long.
- [CompositeReceiveFileJob](https://github.com/KDE/kdeconnect-android/blob/f753fddd2a62676ff0537fd28e191f0ec4c50d9e/src/main/java/org/kde/kdeconnect/plugins/share/CompositeReceiveFileJob.kt)
  recibe hacia un OutputStream y mantiene los totales con Long. El límite de tamaño
  del JSON de control no es el límite del archivo binario.

No se encontró un tope de 1 GiB en esas rutas. Eso no vuelve infinitos el disco, el
sistema de archivos, el proveedor de documentos de Android o los tiempos de red.
Un destino FAT32, por ejemplo, tiene restricciones distintas de un destino ext4.
No se deduce un límite universal de KDE a partir de un único teléfono.

## Qué cambió en Umbriel Connect

El límite de 1 GiB de 0.3.0 era una decisión de nuestro código. Se eliminó de emisor
y receptores. Tamaños y offsets siguen siendo de 64 bits, con rango interoperable
exacto de JSON hasta 2^53−1 bytes (casi 8 PiB); no se promete almacenamiento físico
de ese tamaño. Antes de recibir se comprueba espacio disponible, conservando 8 MiB
de margen. La escritura también comprueba errores posteriores del sistema de archivos.

Se conserva SHA256 incremental, escritura atómica/MediaStore pendiente y rechazo de
nombres/rutas/offsets inválidos. Cada bloque tiene hasta 32 KiB; la ventana acotada
pasa a ocho bloques en ambos sentidos. Android ya no espera una ida y vuelta por
cada bloque. El uso de memoria no depende del tamaño total del archivo.

Se muestra progreso en Android hasta cuatro veces por segundo. Mientras hay una
transferencia activa, la ventana visible evita el apagado automático de pantalla,
sin solicitar WAKE_LOCK. Bloquear manualmente, cerrar la app o revocar la conexión
continúa cancelando la transferencia. No se añade recepción permanente en segundo plano.

## El archivo no amplía el control remoto

Al vencer los diez minutos se detienen mouse y permisos de control del teléfono y
la computadora. Solo un archivo **aceptado/iniciado antes del vencimiento** puede
terminar. Los mensajes deben pertenecer al mismo identificador; no se permite iniciar
otro archivo, aceptar una oferta pendiente o ejecutar una orden nueva.

La app muestra «Solo transferencia · control vencido». Al terminar el último archivo
se cierra ese transporte. Se mantienen revocación, verificación TLS, secuencias y
cancelación al bloquear/salir. Una transferencia sin progreso durante 30 s falla;
el alcance exclusivo de archivos tiene un máximo de 24 h desde la autenticación.
Los receptores/app 0.3.0 siguen necesitando actualizarse para archivos grandes.

El control de diez minutos no se modifica. La variable local UMBRIEL_REMOTE_CONTROL_MS
permite acortarlo (100–600000 ms) para pruebas aisladas; nunca puede aumentarlo.

## Diferencia que permanece respecto a KDE

KDE emplea un transporte binario de contenido separado. Nuestro canal propio aún
usa fragmentos Base64 en JSON sobre la conexión TLS existente: añade aproximadamente
un tercio de bytes al contenido, además de cabeceras. Esto conserva el único puerto
privado usado por la VPN de usuario, sin abrir puertos de payload adicionales.
No se anuncia como equivalente en velocidad/CPU ni como interoperable con el
protocolo de archivos de KDE. No hace falta instalar KDE para este envío.

## Pruebas

La suite comprueba ofertas mayores de 1 GiB y 4 GiB, precisión de offsets, falta de
espacio, hashes y limpieza. Las pruebas TLS acortan el permiso y comprueban que el
archivo aprobado termina mientras acciones, mouse y archivos nuevos son rechazados.
TransferScope se ejercita también como clase Java independiente del entorno Android.

Prueba grande opcional, con identidades/archivos sintéticos temporales y memoria
acotada; no se ejecuta en cada CTest:

```bash
dbus-run-session -- python3 tests/large-files.py --binary build/umbriel-connect-remote
# Cruza también el límite de contadores de 32 bits:
dbus-run-session -- python3 tests/large-files.py --binary build/umbriel-connect-remote --bytes 4294967313
```

El origen usa un archivo disperso; el receptor escribe el contenido real en una
carpeta temporal y se verifica el hash también desde disco. El script elimina sus
archivos al terminar. La prueba utiliza un cliente simulado, no el Samsung físico.
Las mediciones se registran en VALIDATION.md y en la documentación funcional local.
