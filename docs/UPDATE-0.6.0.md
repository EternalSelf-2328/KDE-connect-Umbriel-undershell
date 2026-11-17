# 0.6.0 — vinculación local por código y escritorio propio

2026-11-17 · Android versionCode 10 · mismo paquete y certificado de publicación.

## Conectar después de borrar los datos

1. Actualiza la computadora y el [APK actualizado](UPDATE-0.6.1.md).
2. Abre **Inicio** en Android, con ambos equipos en la misma red. Busca la computadora
   y pulsa **Vincular** en su tarjeta.
3. Compara el código del teléfono con la solicitud que aparece en **Inicio** de la PC.
   Si coinciden, pulsa **Coinciden · vincular** en la computadora.
4. Permite la notificación de conexión. Después se reconecta automáticamente en LAN;
   fuera de ella sigue requiriendo huella o PIN/patrón/contraseña del teléfono.

No se necesita importar un archivo para este flujo. El QR/JSON queda como alternativa
en **Ajustes → Vincular con QR…** del escritorio. Un `pairing.json` caduca a los
cinco minutos y se consume una vez. Borrar los datos Android elimina su confianza:
no se reutiliza un registro antiguo ni se recuperan silenciosamente claves borradas.
Actualizar el APK sobre la instalación existente conserva los datos; no los borres.

La detección por broadcast requiere una LAN que permita comunicación entre clientes.
La búsqueda usa la interfaz Wi-Fi/Ethernet física, sin cambiar las rutas de Tailscale,
y reintenta tres veces. Las respuestas son candidatos sin confianza: no autorizan
controles. El receptor devuelve nombre, dirección e identidad; tras solicitar el enlace,
la PC exige confirmación del código. Si la red aísla clientes o bloquea broadcast,
el QR/archivo y dirección local manual siguen siendo alternativas.

## Retirar conexiones antiguas

Selecciona una tarjeta en Inicio y pulsa **Olvidar dispositivo…**. Para el canal de
controles se revoca el grupo de claves y se cierran sus sesiones. Para las funciones
del motor se retira el vínculo y se oculta esa identidad; un anuncio sin vínculo
simplemente se oculta. **Ajustes → Mostrar dispositivos ocultos** recupera los anuncios.
No se borran archivos ni se fusionan identidades por nombre/IP. Controles y funciones
siguen teniendo confianza independiente; una coincidencia de nombre no prueba identidad.

## Escritorio

Navegación lateral persistente **Inicio / Mi escritorio / Fondos / Teléfono / Ajustes**,
tarjetas de dispositivo horizontales, acciones y envío en Inicio. La configuración
general heredada deja de ser un destino: **Funciones del teléfono…** abre un diálogo
Qt propio de interruptores por función, con la paleta del programa. Algunas ventanas
especializadas, como conversaciones, siguen utilizando el backend/interfaz derivados.
El receptor y los plugins continúan siendo código derivado de KDE, con sus créditos;
no se afirma haber reescrito todos los motores ni probado todos sus plugins físicos.

Buscar dispositivos ya no depende de que una consulta previa haya marcado el motor
como disponible: solicita su refresco y consulta el receptor local, con estado de
búsqueda y error si no escucha el descubrimiento. Al aprobar un teléfono se selecciona
su vínculo nuevo. Flechas/IJKL, Tab, Alt+1…5 y Ctrl+K se conservan.

## Seguridad de la primera vinculación

La solicitud solo se admite por LAN física directa, con TLS y prueba de posesión de
la clave LAN P-256. El código de 48 bits se deriva del certificado TLS, ambas claves
LAN/remota y un nonce aleatorio de servidor; no es un PIN constante ni una contraseña.
La aprobación llega por el socket Unix privado, comprobando UID. Hay caducidad de
dos minutos, límite de solicitudes, claves distintas y ninguna sobrescritura de
confianza existente. Se conserva el permiso opt-in de controlar el celular: una
vinculación nueva lo deja desactivado. El código debe compararse, no solo aceptarse.
No se crea un permiso remoto sin autenticación: esa clave sigue protegida por Android.

Se declara `ACCESS_LOCAL_NETWORK` para Android17/API37 y se pide al buscar cuando
corresponde. [Regla oficial de Android](https://developer.android.com/privacy-and-security/local-network-permission).
El Galaxy informado por el usuario es API36; no se afirma haber probado Android17 físico.

Pruebas: 31 casos TLS reales en directorios temporales, 33 pruebas Android únicas,
regresiones Qt de teclado/adaptación, manifiesto/DEX/firma y bytecode R8 del APK final.
Las imágenes de [la interfaz](screenshots/0.6.0/) son renders sintéticos; no capturan
el escritorio privado ni muestran una conexión física al Galaxy. Prueba Samsung pendiente.
