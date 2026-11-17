# Bloqueo de instalación y reducción de permisos

El 2026-11-15 el usuario mostró un bloqueo de Google Play Protect al instalar
Umbriel Remote 0.1.0 en su Galaxy A24. La advertencia menciona acceso a datos sensibles
y riesgo de robo de identidad o fraude financiero. No se copia la imagen al repo.

## Diagnóstico

La [guía oficial de Google](https://developers.google.com/android/play-protect/warning-dev-guidance)
describe ese aviso para apps descargadas por navegador, mensajería o administrador
de archivos que declaran accesibilidad, lectura SMS o un lector de notificaciones.
El APK 0.1.0 declaraba un servicio protegido por BIND_ACCESSIBILITY_SERVICE, con
lectura de ventanas, gestos y capturas. No declaraba SMS ni lector de notificaciones.
La coincidencia explica una causa muy probable del bloqueo; no tenemos registros
internos de la clasificación de Google ni una auditoría de seguridad externa.

La firma APK válida comprueba la identidad/integridad, pero no otorga aprobación
Google ni garantiza que una app sea segura. El texto mostrado tampoco permite
concluir por sí solo que se detectó una familia concreta de malware.

## Cambio real de alcance en 0.1.1

| Edición | Teléfono→computadora | Computadora→teléfono | Accesibilidad |
|---|---|---|---|
| `desktop`, descarga publicada | Acciones/perfiles con huella | No disponible | Ausente del manifiesto y del DEX |
| `phone-control`, código opcional | Acciones/perfiles con huella | Implementado, sin prueba física | Presente, requiere revisión/consentimiento |

La descarga mínima conserva TLS con certificado fijado, AndroidKeyStore,
BiometricPrompt fuerte y la sesión limitada. No incluye el servicio/código de
capturas o gestos ni la configuración XML del servicio de accesibilidad. El receptor Android fuerza
phoneControl=false, incluso si un Intent intenta solicitarlo, y su backend de
control inverso devuelve un rechazo. No hay descarga de plugins o activación
oculta de esas capacidades después de instalar.

La variante con accesibilidad conserva su código en android/variants/phone-control,
con declaración y consentimiento explícitos. No se presenta como herramienta para
personas con discapacidad ni se usa isAccessibilityTool=true. No se publica un APK
alternativo para sortear el bloqueo. Ambas variantes conservan paquete/identidad
y no se pueden instalar simultáneamente. El versionCode sube de 1 a 2.

Se retiró la descarga 0.1.0 bloqueada. Sigue en el historial Git como evidencia,
pero no debe recomendarse para la instalación actual.

## Verificado y pendiente

Ambas variantes se compilaron con SDK 36 y firma v3 verificada. Se inspeccionaron los
manifiestos binarios y DEX reales de los APK; la edición mínima tiene exactamente
Internet, biometría, notificaciones y los permisos de servicio conectado. Se
comprobó que no contiene la clase AccessibilityService, el backend PhoneAccessibility
ni res/xml/accessibility.xml. Se ejecutó el rechazo del backend mínimo y las 20
comprobaciones SessionGuard. La identidad de firma coincide con el APK 0.1.0.

**Sin verificar:** instalación del nuevo APK con Play Protect activo, UI física,
huella, red y gestos reales. No se puede garantizar que reducir los permisos elimine
el bloqueo: Google puede considerar señales adicionales o una clasificación previa.
No se cambian Play Protect, el paquete, el firmante ni el origen para disimular la app.

```bash
bash android/build.sh desktop
bash android/build.sh phone-control
python3 tests/android-artifacts.py
bash tests/android-guard.sh
```

La preparación limpia las clases/DEX de cada variante por separado para evitar
que una compilación anterior incluya accidentalmente el servicio excluido.

## Revisión oficial

Si continúa el bloqueo, conservar Play Protect y revisar las
[reglas de permisos sensibles](https://support.google.com/googleplay/android-developer/answer/16558241).
El desarrollador puede usar el
[formulario oficial de revisión](https://support.google.com/googleplay/android-developer/contact/protectappeals).
Pide paquete, hash del APK y datos de contacto; comprobar los requisitos actuales
antes de enviarlo. No se envió una apelación ni se contactó a Google desde esta sesión.

Datos públicos para revisión: paquete org.eternalself.umbrielremote y hashes en
[SHA256SUMS](../downloads/SHA256SUMS). Una revisión o distribución futura no garantiza
aprobación y requiere su propia comprobación. No desactivar Play Protect ni habilitar
permisos por ADB para completar estas pruebas.

## Aplicación completa 0.4.0

A petición del usuario, se incorpora un motor Android propio derivado de las fuentes
oficiales. Esta edición **sí** declara las capacidades opcionales de SMS/contactos,
lector de notificaciones, accesibilidad y almacenamiento necesarias para esas funciones.
La edición mínima0.3.1 sigue disponible por separado; ambas usan el mismo paquete y
firmante y son alternativas, no dos apps instaladas a la vez.

Los permisos se conceden por función y el intercambio requiere autorización local;
la biometría no elimina los avisos de Google ni acredita la app ante Play Protect.
No se distribuye como método para evadir ese bloqueo. No se ha confirmado instalación
de0.4.0 en el Galaxy. Si se bloquea, mantener la protección activa y seguir el proceso
oficial de revisión; no activar permisos restringidos para sortear un diagnóstico.
