# Red local, archivos propios y respuesta del mouse — 0.3.1

> El canal descrito aquí sigue disponible. Para la aplicación completa0.4.0 y su motor privado, consulta [seguridad](../SECURITY.md) y [matriz actual](COMPATIBILITY.md). Las referencias a aplicaciones KDE son históricas; la ruta nueva no las ejecuta.

En la misma red **no hace falta Tailscale**. El receptor ofrece TLS en las IPv4
privadas reales de las interfaces activas (RFC1918), además del loopback usado
por el proxy privado de Tailscale. No se modifican rutas, DNS, firewall ni servicios
de inicio automático. Al abrir la ventana se inicia el receptor si no está activo;
la identidad debe haberse creado durante el registro inicial.

## Conexión y seguridad

Android intenta descubrir la computadora registrada por UDP en el puerto TLS+1
(31595 por defecto), verifica nonce/huella y después establece TLS con el certificado
fijado. La respuesta de descubrimiento **no concede confianza**. Se prueban la IP
local recordada y finalmente el destino del registro (por ejemplo Tailscale).
El descubrimiento refresca las interfaces privadas antes de responder; no sondea
la red en reposo. Al volver a autenticar se actualiza el nombre del teléfono registrado.

El transporte TLS se enlaza a direcciones concretas, nunca a todas las interfaces.
El socket UDP de descubrimiento escucha IPv4 y solo responde a remitentes RFC1918;
publica identidad pública y direcciones privadas, nunca claves ni registro, y
limita las consultas a veinte por segundo. La
configuración sigue admitiendo loopback para pruebas/proxy y tailscale0 de sistema.

Se conservan certificado fijado, desafío firmado P-256, biometría fuerte, secuencia,
revocación, control de hasta diez minutos y cierre al bloquear o salir de la app.
Solo un archivo ya aprobado puede terminar después del vencimiento del control;
[alcance y límites de archivos grandes](LARGE-FILES.md).
La conexión local no reduce la autenticación. Las redes de invitados, aislamiento
entre clientes o un firewall pueden bloquear la detección o TCP. En ese caso:

1. En escritorio: **Acceso protegido → Ver conexión local**.
2. En Android: **Configurar red local** e introducir esa IP. Conserva el certificado.
3. Si el router impide comunicar los equipos, usar una red que lo permita o Tailscale.

Para iniciar únicamente la conexión local, sin una VPN:

```bash
~/.local/bin/umbriel-remote-session lan
~/.local/bin/umbriel-connect-remote network
```

El registro ya importado y su clave biométrica sirven también en LAN; actualizar
el APK y el receptor no requiere borrar datos ni volver a registrar el teléfono.
Un registro nuevo puede generarse con `pair IP_LOCAL`, usando una IP privada real
del equipo; el archivo sigue siendo privado, de un uso y válido cinco minutos.

## Mouse

En 0.2.0 se esperaba la respuesta de cada movimiento, con un máximo de 20 Hz y
un retardo de agrupación de 50 ms. La nueva ruta agrupa a 16 ms (hasta unos 60 Hz),
no espera respuesta por desplazamiento y activa TCP_NODELAY en ambos extremos.
El búfer de movimiento es acotado: no crece una cola de paquetes antiguos si la red
se retrasa. También se envía el último movimiento antes de levantar el dedo.

La sensibilidad inicial pasa a 2,4, admite 0,5–4 y recuerda lo elegido. Se conserva
clic/rueda/arrastre y la aprobación de Umbriel. No hay sondeo del touchpad inmóvil;
las fracciones que aún no forman un desplazamiento no mantienen un temporizador.
El TCP compartido puede sufrir latencia durante pérdida de paquetes o transferencias;
no se promete una latencia física que no se haya medido en el Galaxy.

## Fondos

Los filtros son independientes: **Mis carpetas / Wallpaper Engine / Todos los
orígenes**, carpeta, tipo (imagen/video/escena), búsqueda y favoritos. Se recuerdan
origen, carpeta y tipo. Por defecto se muestran los fondos locales. La carpeta se
deriva del `path` del proveedor; los nombres de archivo en `key` no bastan. Los
videos de Wallpaper Engine se clasifican por su origen, no como videos locales.

La consulta real de este equipo encontró 276 imágenes en la carpeta `wallpapers`
y 186 fondos procedentes de Wallpaper Engine. Son cifras de esa consulta, no
límites del producto. Los filtros remotos usan identificadores opacos y nombres de
carpeta; no se envían rutas absolutas. Las categorías reflejan la biblioteca del
proveedor; no mueven archivos ni editan sus etiquetas o carpetas.

Miniaturas, favoritos, paginación de doce, vista ampliada y confirmación permanecen.
Aplicar afecta a todas las pantallas; aún no hay selector por monitor ni subida de
fondos a la biblioteca. Transferir una imagen a Descargas no la importa como fondo.

## Archivos sin KDE Connect

El escritorio combina las sesiones de **Umbriel Remote** con la compatibilidad
KDE. Los dispositivos registrados de Umbriel permanecen visibles desconectados;
al aprobar con huella se actualizan por eventos. Su envío **no utiliza kdeconnectd,
kdeconnect-handler ni la app KDE del teléfono**. No se elimina KDE Connect ni se
alteran sus emparejamientos: sus otras funciones continúan disponibles por separado. Consultar la lista no inicia
automáticamente kdeconnectd; para usar la compatibilidad, iniciar su cliente/daemon.

- PC → teléfono: **Dispositivos → teléfono Umbriel → Enviar archivo**, o el selector
  independiente `umbriel-connect --send ARCHIVO...`. Aceptar cada descarga en Android.
- Teléfono → PC: pestaña **Archivos → Elegir archivo**. El selector de Android cierra
  la sesión de control; al volver pide huella. Después, **Enviar archivo seleccionado**.
- Las descargas se guardan en `Descargas/Umbriel Connect` del dispositivo receptor,
  con un prefijo aleatorio para preservar los archivos existentes.
- Transferencia por bloques de hasta 32 KiB, ventana de ocho bloques en ambos sentidos, tamaños/offsets validados y verificación SHA256 al terminar. Sin el antiguo tope de 1 GiB;
  espacio libre y rango JSON de 64 bits comprobados. Una transferencia por sentido. Sin reanudación automática.
  Se separa el presupuesto de mensajes de archivos (4096/s) del de controles (180/s),
  para no cerrar una transferencia rápida de LAN con el límite pensado para acciones.
  La confianza se consulta en caché durante 250 ms y los cambios del almacén fuerzan
  revalidación inmediata y revocación.
- Android utiliza MediaStore Downloads con archivo pendiente; no añade permisos de
  almacenamiento amplio o accesibilidad. El receptor usa escritura atómica y 0600.
- Rechazo, desconexión o vencimiento cancelan la operación y limpian las descargas
  pendientes durante un cierre normal. Un proceso terminado abruptamente puede
  dejar temporales; no se anuncia un archivo incompleto como recibido.

Mantener Umbriel Remote abierto y aprobar la sesión sigue siendo necesario. No se
habilitó recepción permanente en segundo plano ni encendido remoto de la computadora.
Enlaces, texto y exploración SFTP de la ruta antigua siguen siendo funciones KDE;
esta ampliación sustituye el envío de **archivos**.

## Yazi

```bash
~/.local/bin/umbriel-connect --send '/ruta/archivo con espacios' /otra/ruta
~/.local/bin/umbriel-connect share remote:ID /ruta/archivo
```

En la instalación del usuario, Ctrl+K y el opener **Enviar por Umbriel Connect**
usan `--send %s`. La selección múltiple conserva cada argumento y no interpreta
los nombres como comandos. Esta integración no cambia los demás atajos/openers.

## Verificación y límites

Las pruebas usan un bus privado, identidades y archivos temporales. Cubren TLS,
LAN/discovery, listado/eventos de la sesión propia, CLI de envío, hashes, tamaños,
rutas, rechazo y desconexión. Yazi se ejercita de verdad en PTY con un emisor falso,
incluidos varios archivos, espacios, apóstrofos y metacaracteres. Se verifican las
dos variantes APK firmadas y los mismos permisos mínimos de la descarga.

Sin verificar físicamente: sensación del mouse en el Galaxy, conexión a través del
router real, apariencia de la nueva UI Android, MediaStore en Samsung, archivos de
gran tamaño y cambios reales de fondo. No se movió el puntero ni se cambió el fondo
del usuario durante estas pruebas. No se desactivó Play Protect.
