# Autenticación local y remota — 0.4.1

Actualizado el 2026-11-17. Equipo objetivo: Galaxy A24, Android16/OneUI8.5 informado
por el usuario. No se guardan identificadores personales del teléfono en el repo.
[Seguridad general](../SECURITY.md) · [Actualizar](UPDATE-0.4.1.md).

## Política de acceso

- **LAN directa:** dispositivos previamente vinculados, TLS y firma de una clave
  LAN del Keystore. No hay confirmación humana por conexión. Continúa en segundo
  plano con notificación visible y reconexión de espera acotada a2–30 s.
- **Fuera de LAN:** firma con otra clave del Keystore, autorizada por uso mediante
  huella fuerte o PIN/patrón/contraseña del teléfono. Máximo10 min de control; al
  bloquear se revoca. No se guardan ni se transmiten esos secretos.
- **Siempre:** certificado de PC fijado, identidad del teléfono registrada, desafío
  único, comprobación de capacidades, secuencias crecientes y revocación.

La LAN se clasifica con la ruta del socket, nunca con nombres Wi-Fi o del dispositivo.
En Android requiere Wi-Fi/Ethernet sin transporte VPN y mismo prefijo privado; los
sockets locales se enlazan expresamente a esa red. La PC requiere IPv4 RFC1918 y
mismo prefijo en una interfaz física activa. Loopback y Tailscale no reciben trato
LAN. Una VPN con direcciones privadas tampoco basta para omitir la autenticación.
Es una comprobación de ruta, no una prueba criptográfica de ubicación: no publicar
puertos del receptor en Internet ni introducir un proxy que haga pasar tráfico remoto
por una conexión LAN. [Suposiciones y permisos](../SECURITY.md).

Al terminar el permiso remoto se espera únicamente una LAN directa; nunca se abre
un nuevo permiso remoto sin intervención. «Pausar conexión» suspende la reconexión
hasta Conectar. Android puede limitar el proceso por batería, forzar su cierre o
impedir el inicio tras detener manualmente la aplicación: no se promete ejecución
ininterrumpida frente a esas decisiones del sistema.

## Registro y actualización

El registro es un JSON privado0600 de un solo uso y cinco minutos de vigencia.
El teléfono comprueba la huella SHA256 TLS y la vigencia del certificado. La PC
consume el secreto de256 bits antes de crear confianza. El descubrimiento UDP solo
localiza la PC; no acepta una nueva identidad.

En 0.4.1 existen dos claves EC P-256 de Android Keystore:

- `umbriel-remote-credential-v2`: autenticación por operación con
  `AUTH_BIOMETRIC_STRONG | AUTH_DEVICE_CREDENTIAL`.
- `umbriel-lan-device-v2`: firma automática de dispositivo, sin diálogo por uso.

Un registro anterior usa `umbriel-remote-biometric-v1`. Esa clave se creó limitada
a huella y Android no permite cambiar su política. Hace falta una última firma con
ella para delegar las dos nuevas claves; después la LAN es automática y el acceso
remoto admite el bloqueo del teléfono. Si la huella antigua no está disponible,
hace falta registrar de nuevo, no saltarse la verificación.

## Protocolo firmado v2

El desafío es ASCII exacto, incluidos todos los saltos y el final:

```text
umbriel-connect-auth-v2\n
SHA256_HEX_DEL_CERTIFICADO\n
SHA256_HEX_DE_LA_CLAVE_PUBLICA_FIRMANTE\n
phone-control|desktop-control\n
interactive|upgrade|lan-auto\n
SHA256_HEX_DE_LA_CLAVE_LAN\n
SHA256_HEX_DE_LA_CLAVE_REMOTA\n
BASE64_DEL_NONCE_DE_32_BYTES\n
```

En cada línea con alternativas se usa solo el valor elegido. El desafío pertenece a
un socket TLS, vence en60 s y admite un intento. Se firma con ECDSA/SHA256. No basta
un campo `biometric=true`. La clave LAN solo se acepta en modo `lan-auto` sobre la
ruta local; no sirve para `interactive`, `upgrade` ni el protocolo v1 histórico.

La PC guarda alcance `interactive` o `lan`, y relaciona la clave LAN con la identidad
remota principal. La migración liga también el sucesor de la clave antigua. Revocar
la identidad principal invalida los accesos delegados; la clave antigua no puede
volver a crear una identidad revocada. No se duplican filas de un mismo teléfono en
la interfaz. La firma liga ambas claves, modo y capacidades, para impedir sustituir
una delegación en tránsito.

Tras autorizar hay secuencias estrictamente crecientes, lease monotónica600 s y
heartbeat10 s. Solo la sesión LAN puede renovar automáticamente mientras la ruta y
la confianza siguen siendo válidas. Android rechaza autorizaciones prematuras,
repetidas o con otra identidad/capacidad. La sesión remota nunca recibe esa renovación.
El protocolo v1 sigue disponible para clientes históricos sin concederles los permisos
LAN v2.

No hay validación remota de Android Key Attestation: el servidor comprueba las firmas
y el registro, pero no puede demostrar que cualquier cliente sea la app legítima ni
que su clave provenga de hardware seguro. No hay auditoría externa del protocolo.

## Archivos y control del teléfono

En LAN se reciben archivos de la computadora vinculada en Descargas, con nombres,
espacio, offsets y SHA256 comprobados; nunca se abren automáticamente. Fuera de LAN
se pide aprobación por archivo. Solo una transferencia remota ya aprobada puede
terminar después de vencer el control, hasta24 h y con timeout de inactividad, sin
aceptar otros archivos ni nuevas órdenes. [Detalle](LARGE-FILES.md).

El control inverso necesita el servicio de accesibilidad opcional, consentimiento y
teléfono desbloqueado, incluso en LAN. Las capturas usan la API de accesibilidad y
Android puede rechazar ventanas protegidas. No se evade el bloqueo, no se activa
accesibilidad por ADB y no se capturan pantallas automáticamente en estas pruebas.

El motor compatible completo tiene un gate local separado: emparejamiento TLS y
comprobación de LAN por par, o permiso local de identidad de diez minutos. Los plugins
siguen requiriendo sus permisos. No se presenta ese gate como firma remota por paquete.
Los permisos remotos persistentes de 0.4.0 se eliminan al actualizar.

## Receptor y Tailscale

```bash
~/.local/bin/umbriel-connect-remote network
~/.local/bin/umbriel-connect-remote clients
~/.local/bin/umbriel-connect-remote revoke ID
systemctl --user status umbriel-connect-remote.service
```

El receptor escucha loopback y direcciones privadas concretas, nunca `0.0.0.0`.
El descubrimiento UDP31595 solo responde a remitentes privados, limita frecuencia y
no publica claves ni secretos. `UMBRIEL_CONNECT_LAN=0` deshabilita esa ruta. El
socket Unix administrativo es privado y comprueba UID; la orden `stop` no se acepta
desde TLS. Los servicios de usuario se instalan con CMake y se habilitan con
`install.sh --autostart`; no inician Tailscale ni modifican firewall, rutas o DNS.

En LAN no hace falta Tailscale. Fuera de casa, el proxy privado Serve userspace
reenvía TCP sin terminar TLS. No usar Funnel. Para recuperar el helper instalado:

```bash
# Mantener cada comando en su terminal; no cambia DNS/rutas ni activa exit node.
~/.local/bin/umbriel-remote-session vpn
~/.local/bin/umbriel-remote-session start
```

El modo userspace solo transporta el canal propio publicado: no añade automáticamente
SFTP, multicast, SMS del motor, ADB, Bluetooth ni cámara. Limitar en la tailnet los
accesos a los dispositivos elegidos. No regenerar confianza si se conserva la anterior.

## Verificación

0.4.1 pasa las diez suites C++ y las comprobaciones Java del canal. Las pruebas TLS
reales cubren registro v2, migración, firma de delegación, rechazo de clave LAN por
loopback/remoto y v1, renovación local, revocación y cierre administrativo solo local.
Las regresiones Android comprueban el manifiesto foreground, un registro inválido,
Wi-Fi/VPN/celular, pausa, vencimiento y reporte sin mensajes de excepción.

Sin validar en el Galaxy: reproducción exacta del cierre, diálogo Samsung de huella/
PIN, suspensión/batería, transiciones reales entre redes, accesibilidad y transferencias.
El APK firmado conserva identidad de actualización. La firma no certifica que Google
haya aprobado la aplicación.

## Fuentes primarias

- [Autenticación biométrica y credenciales Android](https://developer.android.com/identity/sign-in/biometric-auth).
- [Android Keystore](https://developer.android.com/privacy-and-security/keystore).
- [Tipos de servicios foreground exigidos por Android](https://developer.android.com/about/versions/14/changes/fgs-types-required).
- [Tailscale Serve](https://tailscale.com/docs/reference/tailscale-cli/serve).
