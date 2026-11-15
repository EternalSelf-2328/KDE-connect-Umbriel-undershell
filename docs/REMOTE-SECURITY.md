# Acceso remoto y aprobación biométrica

**Estado: diseño propuesto el 2026-11-15; no implementado ni habilitado.**
La versión 0.1.0 instalada no exige biometría y no establece una VPN por sí sola.

## Objetivo

Conectar los dispositivos por Internet y autorizar explícitamente el acceso desde
el teléfono. Antes de implementar falta confirmar el modelo/versión de Android y
si se requiere controlar la computadora, el teléfono o ambos. El usuario está
fuera de casa y depende del acceso remoto: no modificar red, firewall ni sesión
mientras se define la solución.

## Transporte

Propuesta: una red privada mediante Tailscale/WireGuard y el emparejamiento TLS de
KDE Connect. Limitar la política de la red a los dispositivos y servicios elegidos;
no publicar KDE Connect ni ADB directamente en Internet. Tailscale añade un cliente
en cada dispositivo y una cuenta/coordinación externa. WireGuard administrado
manualmente requiere resolver alcance entre redes/NAT y distribución de claves.
No se ha medido el costo de estas opciones en este equipo ni instalado Tailscale.

La VPN proporciona conectividad y cifrado. La pertenencia a esa red no debe
conceder automáticamente permiso de control. El acceso por IP debe conservar la
verificación de certificados/identidad del emparejamiento, sin aceptar certificados
nuevos silenciosamente. La dirección de red no sustituye la identidad criptográfica.

## Biometría en el teléfono

Android ofrece `BiometricPrompt` y claves de Android Keystore que requieren
comprobación del usuario para cada uso. Para aprobación criptográfica estricta,
usar una operación `Signature` en un `CryptoObject` y un autenticador
`BIOMETRIC_STRONG`. El reconocimiento facial disponible depende de la clasificación
del hardware; no asumir que cualquier desbloqueo facial sirve. Una alternativa con
PIN debe ser una política explícita, no una degradación silenciosa.

El escritorio sigue en C++. La integración con el diálogo y Keystore de Android
necesita una pequeña capa Kotlin/Java o JNI. Los datos biométricos no se envían al
escritorio: este verifica una respuesta firmada con la clave pública previamente
registrada. La política de uso de esa clave se establece en el teléfono.

## Punto de aplicación de la autorización

Se revisaron `Device.kt` y `MouseReceiverPlugin.kt` del cliente KDE Connect Android:
el dispositivo emparejado entrega los paquetes a los plugins activos; el receptor
de mouse depende del permiso de accesibilidad. Esas rutas inspeccionadas no exigen
una aprobación biométrica por sesión.

Por tanto, un diálogo en el frontend de Linux o una app acompañante que solo
devuelva «aprobado» **no protege** las funciones que sigan accesibles directamente
por los plugins oficiales. Es necesario aplicar la política en el receptor de cada
operación y cubrir las rutas alternativas. La opción que conserva las funciones de
KDE Connect es modificar también su cliente Android.

- **Computadora → teléfono:** el receptor Android bloquea las operaciones protegidas
  hasta que se apruebe esa computadora y el conjunto de permisos. Debe cubrir mouse,
  teclado, archivos, notificaciones/datos y cualquier otra función incluida en la
  política, tanto solicitudes entrantes como transmisión de datos protegidos.
- **Teléfono → computadora:** el receptor C++ valida una aprobación criptográfica
  reciente y los permisos antes de ejecutar. Las órdenes estándar de `runcommand`
  y otros servicios no pueden continuar como rutas equivalentes sin protección.
- La app oficial sin modificar no debe presentarse como compatible con el modo
  obligatorio de biometría. Un modo heredado debe quedar identificado por separado.

## Flujo propuesto

1. Registrar dispositivos y claves con confirmación local y verificación visual.
2. Establecer el transporte autenticado y comprobar la identidad emparejada.
3. Pedir acceso mostrando dispositivo solicitante, destino y permisos concretos.
4. Aprobar en Android mediante el diálogo biométrico del sistema y una firma.
5. Verificar aprobación, desafío de un solo uso, destino, solicitante, permisos,
   vencimiento y asociación a la sesión autenticada antes de permitir operaciones.
6. Mostrar acceso activo y una acción visible para revocarlo.
7. Invalidar el permiso al cerrar, vencer, revocar, bloquear el teléfono o perder
   la conexión. Una reconexión no restaura permisos por defecto.

Valores iniciales para revisar: desafío válido durante 60 segundos y autorización
máxima de 10 minutos. No se implementarán formatos o intercambios criptográficos
propios sin revisar bibliotecas, serialización, almacenamiento y modelo de amenazas.
No basta con un campo JSON `biometric=true` ni con comprobar un booleano en la UI.

## Pantalla y control completo de Android

El acceso a datos de KDE Connect y ver/controlar la pantalla son funciones distintas.
Para una implementación Android basada en `MediaProjection`, Android exige permiso
de captura por sesión; la biometría no sustituye ese consentimiento. La inyección
de entrada requiere además una API/permiso permitido por Android. No se ofrecerá
eludir pantalla de bloqueo, diálogos de consentimiento ni permisos del sistema.

La ruta de scrcpy original usa ADB por separado. No se debe afirmar que está
protegida por una autorización del nuevo cliente si ADB conserva un acceso directo.
Debe excluirse del modo biométrico obligatorio o integrarse con una política que
controle efectivamente esa vía.

## Pruebas necesarias antes de habilitar

- Identidad desconocida, certificado cambiado, clave revocada y dispositivo no autorizado.
- Acceso sin huella, cancelación, intentos fallidos y hardware biométrico no compatible.
- Reutilización de aprobación, cambio de permisos/destino/sesión y desafío vencido.
- Reconexión, reinicio, bloqueo del teléfono y revocación durante una transferencia.
- Intento de saltarse el frontend mediante KDE Connect oficial, `runcommand`, ADB
  u otra ruta previamente habilitada.
- Permisos de captura/entrada y sesiones reales sobre datos móviles y otra red Wi-Fi.

Hasta esas comprobaciones, conservar la etiqueta **experimental, no habilitado**.

## Fuentes primarias consultadas

- [BiometricPrompt y autenticadores](https://developer.android.com/identity/sign-in/biometric-auth).
- [CryptoObject y claves por uso](https://developer.android.com/reference/android/hardware/biometrics/BiometricPrompt.CryptoObject).
- [Android Keystore](https://developer.android.com/privacy-and-security/keystore).
- [Consentimiento de MediaProjection](https://developer.android.com/media/grow/media-projection).
- [Tipos de conexión cifrada de Tailscale](https://tailscale.com/docs/reference/connection-types).
- [Políticas de acceso de Tailscale](https://tailscale.com/docs/features/access-control/grants).
- [Despacho de paquetes Android](https://github.com/KDE/kdeconnect-android/blob/master/src/main/java/org/kde/kdeconnect/Device.kt).
- [Receptor de mouse Android](https://github.com/KDE/kdeconnect-android/blob/master/src/main/java/org/kde/kdeconnect/plugins/mousereceiver/MouseReceiverPlugin.kt).
