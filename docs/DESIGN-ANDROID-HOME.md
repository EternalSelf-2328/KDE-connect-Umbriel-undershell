# Android: controles primero

Acción principal: controlar la computadora desde el teléfono, sin navegar por
configuraciones. Superficie: app Android nativa; paleta dinámica clara/oscura con
los roles de UiDesign, iconos de una familia y superficies sólidas sin blur.

```text
Inicio                 Control           Archivos       Ajustes
PC + estado            Mouse             Enviar         Conexión
6 accesos directos     Navegación         Recibir        Dispositivos
Reproducción/volumen    Sonido/apariencia                 Permisos
                       Fondos/widgets                   Diagnóstico
```

La navegación inferior permanece visible. Los accesos de Inicio abren directamente
el control seleccionado. Los ajustes de red, registros y permisos no compiten con
los controles diarios. No se abre el drawer heredado del motor al tocar la notificación.

Estados: sin vínculo (una acción de registro), buscando LAN, LAN conectada sin
diálogo, autorización remota necesaria, autenticando, remoto conectado, pausa,
error recuperable y solo transferencia. Un Wi-Fi cualquiera no demuestra que la
computadora esté en la misma LAN: el receptor sigue verificándose por TLS y ruta.
El botón remoto aparece tras no encontrar la PC local; no renueva acceso remoto solo.
La primera vinculación y la migración de una clave antigua siguen siendo explícitas.

Los controles locales y remotos usan el mismo dashboard. La autorización es estado
del servicio/Keystore, independiente del destino de navegación; cambiar de página
no concede permisos ni destruye una sesión válida. Salir del touchpad detiene entrada.

Validación: flujos de navegación/autenticación, estado de conexión, regreso del
selector de archivos y renders reales de Views con datos sintéticos, en temas claro/
oscuro y tamaños de teléfono/tableta. Los renders no capturan el escritorio privado.

## Evidencia visual y accesibilidad

[Renders sintéticos0.5.0](screenshots/0.5.0/): Inicio claro/oscuro, Mouse, Apariencia,
Archivos, Ajustes, autorización remota, primer registro, ancho compacto320dp,
tableta840dp y texto 130%. Se dibujaron las Views reales en Robolectric/API 36 con
runtime gráfico nativo y fuentes; no se generaron imágenes de interfaz con IA.
Las capturas son sintéticas y no contienen direcciones, registros ni archivos privados.

| Contraste medido en el render | Oscuro | Claro |
|---|---:|---:|
| Texto / superficie |13.84:1|14.87:1|
| Texto secundario / superficie |8.80:1|6.57:1|
| Texto / acción primaria |12.37:1|6.46:1|

Se cumplen 4.5:1 en esos roles/paletas medidos. El acento dinámico de otros teléfonos
debe revisarse físicamente; no se afirma haber probado todos los fondos Samsung.
Los iconos tienen etiquetas accesibles, las acciones tienen objetivo táctil mínimo
48 dp, y el foco/selección no dependen solo del color. El texto ampliado puede requerir
scroll; no se esconde la barra de navegación. La fila multimedia usa iconos para
evitar que etiquetas largas se partan dentro de una palabra.

Referencia de organización: [patrones oficiales de navegación Android](https://developer.android.com/design/ui/mobile/guides/layout-and-content/layout-and-nav-patterns).
Se consultó Stitch como referencia solicitada, pero no había una herramienta Stitch
conectada: no se subió código privado ni se atribuye el diseño a ese servicio.

Auditoría: navegación validada entre las cuatro secciones, acceso directo al touchpad,
parada de entrada al salir, conservación de la conexión, autorización remota solo
tras búsqueda local y separación de la primera confianza. Enlaces externos de registro
requieren confirmación y cancelar no modifica una confianza previa. No se transfieren
permisos de controlar el teléfono a una computadora con otro certificado.
