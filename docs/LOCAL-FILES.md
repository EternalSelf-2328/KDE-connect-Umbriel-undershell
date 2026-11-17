# Red local, archivos propios y respuesta del mouse — 0.4.1

Desde 0.4.1 la LAN directa usa conexión automática con autenticación criptográfica del
dispositivo, sin pedir huella. Fuera de LAN se exige huella fuerte o PIN/patrón/
contraseña del teléfono por hasta diez minutos. Un registro antiguo puede necesitar
una última huella para migrar sus claves. [Actualización](UPDATE-0.4.1.md) y
[protocolo/seguridad](REMOTE-SECURITY.md).

En la misma red **no hace falta Tailscale**. TLS31594 escucha en las direcciones
privadas reales y loopback; UDP31595 descubre la identidad ya registrada sin conceder
confianza nueva. Android verifica el certificado y enlaza la conexión a Wi-Fi/Ethernet
sin VPN; la PC comprueba la ruta física y el mismo prefijo. No basta una IP privada
obtenida por VPN. No se abren puertos del router, rutas, DNS ni firewall.

Los servicios opcionales de usuario arrancan con el escritorio. Android mantiene
notificación visible y reintentos locales acotados a2–30 s en segundo plano; «Pausar
conexión» los suspende. Las restricciones del sistema/Samsung aún necesitan prueba
física. El control del teléfono sigue exigiendo desbloqueo y permiso explícito.

Si la detección falla por firewall o aislamiento Wi-Fi:

1. Escritorio: **Acceso protegido → Ver conexión local**.
2. Android: **Configurar red local**, introducir esa IP; se conserva el certificado.
3. Si el router bloquea la comunicación, usar una red que la permita o Tailscale.

```bash
~/.local/bin/umbriel-connect-remote network
~/.local/bin/umbriel-remote-session lan
```

El registro inicial es privado, válido cinco minutos y de un uso. No se necesita
registrar de nuevo para actualizar normalmente; no borrar datos de Android.

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

El escritorio combina el canal propio y el motor privado de Umbriel Connect. No
necesita `kdeconnectd`, `kdeconnect-handler` ni la app oficial del teléfono.
Los dispositivos registrados permanecen visibles desconectados y actualizan su
estado al conectar.

- PC → teléfono: **Dispositivos → teléfono → Enviar archivo**, o
  `umbriel-connect --send ARCHIVO...`. En LAN se guarda sin abrirse; fuera de ella
  Android pide aceptar cada descarga.
- Teléfono → PC: **Archivos → Elegir archivo → Enviar archivo seleccionado**.
  Volver del selector reutiliza una sesión que aún esté autorizada.
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

La conexión LAN puede recibir en segundo plano y con pantalla bloqueada. Fuera de
ella hace falta una autorización remota vigente; los archivos ya aprobados tienen el
alcance separado descrito arriba. Enlaces, texto y SFTP también están disponibles en
el motor privado completo, con sus permisos y restricciones de transporte.

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
