# 0.4.2 — cierre al vincular y dispositivos detectados

Actualizado el 2026-11-17. Android versionCode8; mismo paquete y firmante. Se conserva
la política LAN automática y credencial remota de0.4.1.

## Cierre confirmado por el informe del Galaxy

El informe0.4.1/API36 muestra `NoSuchFieldError` durante la comprobación de LAN de
una solicitud de emparejamiento. No identifica un error del lector de huellas.

La dependencia SMS/MMS incluye copias antiguas de `android.net.LinkAddress` y otras
clases de red. R8 había sustituido la llamada pública `LinkAddress.getAddress()` por
una lectura directa de su antiguo campo `address`. En el teléfono se carga la clase
del sistema, cuyo diseño interno es diferente. El APK0.4.1 contiene esa instrucción:

```text
iget-object ..., Landroid/net/LinkAddress;->address:Ljava/net/InetAddress;
```

Las reglas nuevas conservan la frontera de las clases de red y del canal propio:
no se permite esa sustitución por campos internos. El APK se sigue reduciendo y
optimizando fuera de esas fronteras. No se retiran controles de autenticación ni
se convierte una ruta remota en LAN para evitar el cierre.

La verificación de publicación ahora desensambla el **APK release optimizado** y
exige llamadas públicas `getAddress()`/`getPrefixLength()`, sin lecturas de campos
privados de LinkAddress/LinkProperties en la comprobación de LAN. Las pruebas de
UI también ejercitan el retorno de huella, la transición a credencial y el dashboard.
Las pruebas debug anteriores no cubrían esta transformación de R8.

[Reglas y modificadores oficiales de R8](https://developer.android.com/topic/performance/app-optimization/add-keep-rules).

## Identidades y conexiones

La interfaz ya no llama «Conectado» a un anuncio de red sin vínculo. Ahora muestra
**Disponible · sin vincular**. Solo una identidad emparejada y alcanzable aparece
conectada. La lista cuenta identidades, no afirma que todas sean teléfonos físicos.

Un teléfono con KDE Connect oficial y Umbriel puede anunciar dos identidades distintas.
El canal propio firmado tiene además su registro independiente. No se fusionan por
nombre o por IP, porque eso no demuestra que compartan una identidad autorizada.
No se hereda confianza de un anuncio ni se borran claves para eliminar una fila.

Un anuncio sin vincular cuyo destino es una dirección de esta computadora se excluye
del cliente: otro daemon local no es un teléfono externo. Las identidades ya
vinculadas se conservan para revisión y revocación explícita.

En la instalación personal se detectó que el daemon oficial KDE había vuelto a
iniciarse por D-Bus. Se detuvo únicamente ese proceso y se añadió una anulación de
activación de usuario. Se mantienen paquetes, bibliotecas y confianza histórica.
El motor de Umbriel sigue usando su propio nombre D-Bus y sus servicios.

## Actualizar

Instalar [la actualización vigente](UPDATE-0.5.0.md) encima de
la app anterior, conservando datos. Actualizar/reabrir la ventana de escritorio para
cargar los estados nuevos. Si se mantiene KDE Connect en el teléfono, su anuncio
puede seguir apareciendo: cerrar o retirar esa app oficial cuando ya no se use.

La corrección del APK se comprueba sobre el fallo reportado y el bytecode release;
falta confirmar el nuevo APK en el Galaxy real. No se anuncian huella, emparejamiento
y todas las funciones físicas como verificados solo por pasar pruebas automatizadas.
