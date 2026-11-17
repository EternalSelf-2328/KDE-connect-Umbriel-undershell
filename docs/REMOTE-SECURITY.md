# Canal remoto con aprobación biométrica

**Estado actualizado el 2026-11-16: versión 0.3.1, experimental.** El APK 0.1.0 fue
bloqueado por Play Protect; la edición mínima 0.1.1 después registró una sesión
Android autorizada. La 0.3.0 conserva permisos mínimos y añade controles de
computadora, touchpad y galería; su nueva UI/entrada física siguen pendientes de
prueba en el Galaxy. El control inverso continúa como variante de código separada,
pendiente de revisión. [Diagnóstico](PLAY-PROTECT.md) y [alcance actual](REMOTE-CONTROLS.md).

Equipo objetivo informado por el usuario: **Galaxy A24, Android 16, One UI 8.5**.
No se registran IMEI, serie, número telefónico ni capturas de información personal.
La clasificación biométrica real se comprueba en ejecución con `BIOMETRIC_STRONG`.

## Qué está implementado

- **Android → computadora:** app `Umbriel Remote` con los controles de Umbriel,
  Noctalia y undershell y carga de perfiles, mediante el receptor C++.
- **Computadora → Android (variante `phone-control`, no incluida en la descarga mínima):** ventana de control con Atrás, Inicio, Recientes,
  capturas manuales y toques sobre la captura. CLI adicional para gestos y texto en
  campos editables que no sean contraseñas. Requiere permiso explícito de Android.
- TLS 1.2 o posterior, certificado de computadora fijado al registrar, clave EC
  P-256 de Android Keystore que exige biometría fuerte por uso y autorización de
  diez minutos de control como máximo. Solo los archivos previamente aprobados pueden
  terminar después, con alcance limitado por identificador; [detalle](LARGE-FILES.md). No hay sustitución silenciosa por PIN o biometría débil.
- Rechazo de firmas inválidas, desafíos de otra conexión, permisos alterados,
  identidades no registradas, repetición de mensajes y autorizaciones repetidas.
- Cierre al desconectar, bloquear/apagar pantalla del teléfono o revocar la clave.
  Al vencer el permiso se detienen los controles; solo puede terminar un archivo
  ya aprobado (máximo24 h, sin aceptar otros archivos ni órdenes nuevas). Aviso persistente en Android con botón Desconectar.

Son controles y capturas bajo demanda. **No es todavía un escritorio remoto con
video continuo, streaming de audio ni teclado general de la computadora. El touchpad
está disponible con los permisos de Umbriel.**
Los plugins habituales de KDE Connect conservan sus permisos propios y no quedan
protegidos por este canal. Tampoco se bloquean automáticamente ADB, scrcpy, SFTP,
notificaciones o accesos remotos que ya existan.

## Instalar la app Android

APK mínimo de prueba: [UmbrielRemote-0.3.1.apk](../downloads/UmbrielRemote-0.3.1.apk).
Esta edición no declara accesibilidad y no permite controlar ni capturar el teléfono.
La 0.3.0 añade mouse de computadora, controles dedicados y fondos; [uso y límites](REMOTE-CONTROLS.md).
Está firmado con una identidad de desarrollo propia que se conserva fuera del
repositorio; la clave privada de firma y su contraseña no están en Git. La app no
pide acceso a contactos, SIM, número de teléfono, cámara ni micrófono.

1. Instalar el APK y permitir notificaciones, para mantener visible la sesión.
2. Conectar ambos equipos a la misma red local o a Tailscale fuera de casa.
3. Importar el archivo de registro privado generado por la computadora.
4. Presionar **Conectar con huella**. La app comprueba biometría fuerte y
   usa `BiometricPrompt.CryptoObject` para producir la firma.
5. La descarga mínima no ofrece control del celular ni un permiso de accesibilidad.
   La variante de código con esa capacidad requiere revisión previa de distribución
   y consentimiento explícito; no continuar si Play Protect bloquea su instalación.

No instalar automáticamente ni habilitar accesibilidad por ADB. Los permisos y la
huella requieren intervención de la persona en el teléfono. Al cambiar las huellas
registradas, Android puede invalidar la clave: borrar los datos de la app, retirar
la identidad anterior en la computadora y volver a registrar explícitamente.

## Receptor de escritorio

Se compila si se dispone de headers OpenSSL 3 o posterior; puede desactivarse con
`-DUMBRIEL_REMOTE=OFF`. La ventana inicia el receptor si no está activo. No crea
servicios de arranque ni reglas de firewall. TLS no se enlaza a `0.0.0.0`; el
descubrimiento UDP solo responde a remitentes privados. [Detalle LAN/archivos](LOCAL-FILES.md).

```bash
~/.local/bin/umbriel-connect-remote init
# Sustituye la dirección por la IPv4 real de tailscale0, si usas Tailscale de sistema.
~/.local/bin/umbriel-connect-remote pair 100.64.1.2
~/.local/bin/umbriel-connect-remote serve 100.64.1.2
```

La dirección ilustrativa no es la de un dispositivo real. El programa exige que
la dirección exista en una interfaz activa: IPv4 RFC1918 para LAN o `tailscale0`
para la VPN de sistema. También acepta loopback
para pruebas y para un proxy privado de Tailscale en modo de usuario. No acepta
interfaces públicas. Al escuchar loopback habilita además las IPv4 privadas activas
para conexión directa; `UMBRIEL_CONNECT_LAN=0` permite desactivar esa ruta. El modo de usuario requiere publicar el TCP
internamente en la tailnet y anunciar su dirección en el registro, manteniendo TLS
sin terminación en el proxy.

`pair` genera `~/.config/umbriel-connect/remote/pairing.json`, privado (0600), válido
cinco minutos y de un solo uso. Transfiérelo por un medio privado; **no agregarlo a
Git ni publicarlo**. El receptor necesita estar en ejecución cuando Android conecte.
La huella TLS del archivo debe corresponder a la identidad creada en esa computadora.

```bash
~/.local/bin/umbriel-connect-remote clients
~/.local/bin/umbriel-connect-remote --phone
~/.local/bin/umbriel-connect-remote phone ID home
~/.local/bin/umbriel-connect-remote phone ID tap 0.5 0.5
~/.local/bin/umbriel-connect-remote revoke ID
```

La ventana también se abre desde Umbriel Connect → Acceso protegido → Controlar
 teléfono con aprobación biométrica. Las coordenadas son proporciones del tamaño
actual del teléfono, sin resolución fija. El CLI `phone ID screen` guarda una
captura privada en la carpeta actual; la ventana mantiene la imagen en memoria.

## Tailscale sin cambiar la sesión remota

Se preparó Tailscale 1.104.7 en `~/.local/libexec/umbriel-connect-tailscale/`.
El helper usa `--tun=userspace-networking`, no acepta DNS ni rutas, no activa SSH
ni un exit node y deshabilita la subida de logs de Tailscale. Requiere que el usuario
inicie sesión en ambos equipos con la misma cuenta. No se utiliza Tailscale Funnel.

```bash
# Solo al instalar o restaurar las herramientas:
python3 scripts/setup-tailscale-user.py
# Terminal 1: deja el cliente de red en ejecución.
~/.local/bin/umbriel-remote-session vpn
# Terminal 2: abre el enlace de autorización que muestre el comando.
~/.local/bin/umbriel-remote-session login
# Cuando ya esté vinculado, genera el registro privado para el teléfono.
~/.local/bin/umbriel-remote-session pair
# Inicia receptor local y proxy TCP de la tailnet; Ctrl+C cierra ambos.
~/.local/bin/umbriel-remote-session start
```

El cliente Android debe usar la dirección de la tailnet del archivo generado por
`pair-proxy`; el receptor escucha solo en `127.0.0.1`. Tailscale reenvía TCP sin
terminar el TLS de la aplicación. Si Tailscale pide habilitar Serve en su consola,
la cuenta requiere ese paso adicional. No se han probado aún los dos extremos por
Internet; falta completar la cuenta y conectar el teléfono físico.

La configuración de acceso de Tailscale debería limitar este puerto a los dos
dispositivos elegidos. Aunque haya otros miembros en la tailnet, la aplicación
mantiene su propio registro y rechaza sus claves desconocidas. Las reglas de la
red y la aprobación biométrica cumplen funciones diferentes.

## Qué verifica cada extremo

La app Android confía exclusivamente en el SHA256 del certificado registrado y
comprueba su vigencia; un certificado cambiado requiere un nuevo registro manual.
La computadora identifica al teléfono por SHA256 de su clave pública SPKI DER y
verifica una firma ECDSA/SHA256. No se usa un campo `biometric=true` como autorización.

El servidor utiliza OpenSSL para aleatoriedad, certificados y verificación de
firmas. TLS se establece mediante Qt/Android. El intercambio de aplicación es
propio de este prototipo y **no tiene auditoría externa**. No se ha implementado
validación remota de Android Key Attestation: se confía en registrar la app legítima
y en la integridad del teléfono; el receptor no puede demostrar por sí solo que
una clave arbitraria fue generada dentro de hardware seguro.

La firma cubre estos bytes ASCII exactos, incluido el salto final:

```text
umbriel-connect-auth-v1\n
SHA256_HEX_DEL_CERTIFICADO\n
SHA256_HEX_DE_LA_CLAVE_PUBLICA\n
phone-control|desktop-control\n
BASE64_DEL_NONCE_DE_32_BYTES\n
```

`phone-control` autoriza además el sentido computadora → teléfono. El desafío nace
para una única conexión TLS, vence en 60 segundos y solo admite un intento de
firma. Tras aprobar, los mensajes llevan secuencia estrictamente creciente. Se
mantiene una única sesión por clave y no se guardan tokens reutilizables de sesión.
La lease usa tiempo monotónico; una reconexión vuelve a exigir huella.

Android rechaza el mensaje `authorized` antes de producir una firma, otro destino
 o permiso y una segunda autorización que intente renovar la duración. La app usa
una clave `AndroidKeyStore` por uso, con invalidación al cambiar biometría y uso
restringido al dispositivo desbloqueado. El archivo de registro tiene un secreto
aleatorio de 256 bits, cuyo hash se consume antes de crear confianza en el servidor.

## Capturas y accesibilidad

La ruta implementada usa el API oficial `AccessibilityService.takeScreenshot`,
con permiso visible de accesibilidad y aprobación biométrica previa. No usa ADB ni
se instala como servicio oculto. Android puede rechazar capturas y ventanas
protegidas; ese error se informa sin intentar evadirlo. Las capturas se reducen
proporcionalmente a un máximo de 1024 píxeles en el lado mayor para limitar memoria
y tráfico; esa cifra es un límite de calidad, no una resolución asumida del equipo.

Para una futura ruta basada en `MediaProjection`, se mantendrá también el
consentimiento de captura por sesión que exige Android.

## Pruebas realizadas y pendientes

- 12 pruebas de integración con TLS real y firmas EC sobre el receptor C++:
  registro, token de un solo uso/vencido, certificado fijado, ausencia de autorización,
  firma/curva incorrecta, repetición, permisos, acción sobre un socket undershell falso,
  control inverso, reinicio tras interrupción y revocación. Solo sockets y archivos temporales.
- 20 comprobaciones Java sobre la misma `SessionGuard` usada por Android:
  aprobación prematura, identidad/permiso cambiado, repetición, vencimiento,
  reconexión y cierre.
- Compilación contra API 36, empaquetado DEX y firma APK v3 verificada. SDK/JDK de
  fuentes oficiales, descargados y comprobados por checksum. No se necesita Gradle.
- Se mantienen las pruebas QtTest y del widget anteriores.

**Sin verificar físicamente:** clasificación de la huella del A24, primer arranque,
notificación de servicio, restricciones de batería Samsung, accesibilidad, gestos,
rotación y capturas, cierre al bloquear, cambio de red y tráfico por Tailscale.
El túnel/cuenta de Tailscale todavía requieren configuración del usuario. No hubo
capturas del escritorio real ni se enviaron pulsaciones o cambios de red en la sesión.

## Compilar Android

```bash
python3 scripts/setup-android-tools.py
bash android/build.sh desktop
# Variante opcional solo para revisión/desarrollo, puede ser bloqueada por Play Protect:
bash android/build.sh phone-control
python3 tests/android-artifacts.py
bash tests/android-guard.sh
```

Los resultados van a `build/android/desktop/` y `build/android/phone-control/`.
Solo se publica la edición mínima. Las variantes comparten paquete y firma: no
son dos apps instalables simultáneamente. No se descarga ni activa código adicional
en la edición mínima; cambiar de variante requiere un APK diferente.

Las herramientas ocupan una caché de usuario y no cambian Java ni el SDK del sistema.
Se pueden indicar instalaciones propias mediante `UMBRIEL_JDK`,
`UMBRIEL_ANDROID_PLATFORM` y `UMBRIEL_ANDROID_BUILD_TOOLS`. La clave de firma APK y
su archivo de contraseña se crean fuera del repo con permisos privados. No borrarlos
si quieres instalar futuras actualizaciones conservando la misma firma.

## Fuentes primarias

- [BiometricPrompt](https://developer.android.com/identity/sign-in/biometric-auth).
- [Android Keystore](https://developer.android.com/privacy-and-security/keystore).
- [Capturas por accesibilidad](https://developer.android.com/reference/android/accessibilityservice/AccessibilityService#takeScreenshot(int,java.util.concurrent.Executor,android.accessibilityservice.AccessibilityService.TakeScreenshotCallback)).
- [Servicios foreground](https://developer.android.com/develop/background-work/services/fgs/service-types).
- [MediaProjection](https://developer.android.com/media/grow/media-projection).
- [Tailscale Serve](https://tailscale.com/docs/reference/tailscale-cli/serve).
