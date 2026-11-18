# Diseño nativo y móvil

La ventana de escritorio0.6.0 usa navegación lateral persistente, Inicio con tarjetas
de conexiones y acciones directas, y Ajustes separados. La app Android tiene Inicio,
Control, Archivos y Ajustes. [Vinculación y cambios](UPDATE-0.6.0.md) ·
[Diseño Android](DESIGN-ANDROID-HOME.md).

## Escritorio0.6.0

- Paleta Noctalia por roles, variantes claras/oscuras y foco de teclado visible.
- Menú lateral de cinco destinos, sin una segunda barra horizontal de navegación.
- Tarjetas de conexión horizontales, selección por flechas/IJKL y acciones compartidas.
- Solicitud de vínculo con código legible y confirmación explícita en Inicio.
- Olvidar junto al dispositivo; recuperación de anuncios ocultos en Ajustes.
- Configuración de funciones propia en Qt, sin abrir el configurador general heredado.
- Sin blur ni animaciones permanentes; contenido desplazable en ventanas compactas.

[Renders reales con datos sintéticos](screenshots/0.6.0/) en claro/oscuro y formatos
compactos. Se probaron 760×480 hasta1100×720 con Qt offscreen, nunca capturando el
escritorio privado. Las dimensiones son casos de prueba; la ventana usa geometría
lógica real y se adapta a sus límites. Se corrigieron solapamientos de las filas de
navegación detectados al renderizar. La misma paleta central conserva la comprobación
de contraste de texto, texto secundario y acento en las pruebas Qt.

## Evidencia histórica

Los renders anteriores se conservan en screenshots/0.4.0 y0.5.0. La prueba visual
no demuestra que todos los plugins/portales funcionen en el Galaxy o en Wayland.
La instalación física de Android y sus permisos Samsung siguen pendientes.

[Notas originales conservadas](DESIGN-HISTORY.md), con decisiones y renders de versiones anteriores.

## Android0.8.0

Paletas completas por roles: sistema/verde agua/lavanda/rosa/ámbar, con variantes
clara y oscura; texto principal y secundario contrastan al menos4.5:1 con las
superficies verificadas. La personalización y el idioma están en Ajustes; los
controles nuevos están también en Inicio. La barra de cuatro destinos se conserva.
Los renders de [0.8.0](screenshots/0.8.0/) muestran Views reales con datos sintéticos
y títulos de ejemplo, incluyendo inglés y las paletas lavanda y rosa.
