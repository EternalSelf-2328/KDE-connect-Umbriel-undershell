# Compatibilidad y alcance

La versión 0.1.0 es una aplicación nativa funcional sobre el motor oficial. No se
afirma compatibilidad del 100 %: no hubo un teléfono físico disponible en la
sesión de desarrollo del 2026-11-15.

| Función | Ruta nativa | Verificación |
|---|---|---|
| Lista, estado y batería | D-Bus KDE Connect | Teléfono simulado; daemon real con un dispositivo emparejado desconectado |
| Emparejar / aceptar / desvincular | D-Bus; clave visible | Guardas y aceptación simuladas; pendiente Android real |
| Archivos, URL, texto, ping y encontrar teléfono | Plugins oficiales | API verificada; texto/archivo probados con teléfono D-Bus falso |
| Explorar archivos | Plugin SFTP oficial | Método y resultado booleano implementados; pendiente teléfono real |
| Notificaciones y portapapeles | Motor oficial; ajustes por dispositivo | Pendiente envío y recepción reales en Umbriel |
| Audio / MPRIS | Motor oficial | Pendiente control desde Android y publicación de reproductores remotos |
| SMS / contactos | Aplicaciones oficiales, acceso desde la ventana | Lanzador; pendiente teléfono y permisos Android |
| Touchpad / teclado / presentación / tableta | Motor oficial y portales/permisos del sistema | Pendiente comprobar entrada real con Umbriel |
| Control de escritorios y foco | CLI de Umbriel | Sintaxis contrastada con binario; no se cambió la sesión remota |
| DPMS y bloqueo | Umbriel / Noctalia | Implementado; no ejecutado en el escritorio remoto |
| Editor, widgets y rendimiento | Socket de undershell | Solicitudes y errores simulados; no se cambió la configuración real |
| Perfiles guardados | Socket `saves` / `save-load` | Leídos los 18 perfiles reales; carga y errores probados en servidor falso |
| Mascotas / demo / salvapantallas | Socket de undershell | Catálogo y validación; pendiente efecto visual real |
| Órdenes desde Android | Configuración oficial de `runcommand` | Fusión/respaldo/idempotencia/retirada probados en archivos temporales |
| Widget Noctalia | Servicio y widget Luau | Lint válido; pendiente activación y prueba gráfica real |
| scrcpy y cámara/webcam | Código Python original conservado | Sin migrar a interfaz C++; no ofrecer como validado para Umbriel |
| Audio Bluetooth del teléfono | Código original / herramientas del sistema | Sin integrar en la ventana C++; depende del perfil Bluetooth |
| Monitor virtual | Motor KDE o backend original | No validado; original usa Hyprland; Android no ofrece la función de monitor entre computadoras |

## Prueba final cuando el teléfono esté disponible

1. Abrir KDE Connect en ambos dispositivos; comprobar descubrimiento, clave y
   reconexión por Wi-Fi tras suspender la computadora y al cambiar de red.
2. Enviar archivos pequeños/grandes, enlaces y texto en ambas direcciones;
   verificar progreso, cancelación, errores y permisos de almacenamiento.
3. Comprobar batería, llamadas, notificaciones con acciones/respuesta y
   portapapeles, respetando los permisos y restricciones de Android.
4. Probar MPRIS, volumen, teclado con caracteres españoles, touchpad y
   presentación bajo Umbriel. Verificar los permisos del portal antes de concluir
   que un plugin funciona en Wayland.
5. Revisar el catálogo y publicar en el teléfono elegido. En una instancia aislada
   de undershell probar editor, visibilidad, perfiles, rendimiento y mascotas.
6. Probar escritorios en un compositor aislado; DPMS solo sobre una salida virtual.
   No bloquear ni apagar una sesión de la que depende el acceso remoto.
7. Activar el widget y comprobar conexión/desconexión, batería, clic y reinicio del
   daemon, tanto en barra horizontal como vertical.
8. Solo después validar Bluetooth, SFTP, scrcpy y cámara con sus dependencias.

No ejecutar simultáneamente `caelestia-connectd` y `kdeconnectd`: ambas rutas tienen
descubrimiento, certificados y almacenamiento de confianza propios. No se migra
automáticamente una clave privada ni una decisión de confianza entre clientes.
