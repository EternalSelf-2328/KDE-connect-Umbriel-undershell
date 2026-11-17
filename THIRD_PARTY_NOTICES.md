# Procedencia y licencias

Umbriel Connect es un fork GPL-3.0. No representa ni está respaldado por KDE, Caelestia,
Noctalia, undershell u Omarchy. Los autores y sus avisos originales se conservan.

| Componente | Fuente | Revisión integrada/revisada |
|---|---|---|
| Motor C++ KDE Connect | https://invent.kde.org/network/kdeconnect-kde | `642a710abd9dfff4358b5d943ca61965fb34ec6e` |
| Motor Android KDE Connect | https://invent.kde.org/network/kdeconnect-android | `f753fddd2a62676ff0537fd28e191f0ec4c50d9e` |
| Caelestia Connect, base original y extras | https://github.com/maylton/caelestia-connect | `a423a05bbcb762b30c365bf0734478def80d2c5f` |
| Caelestia Connect, pantalla/cámara | https://github.com/KingSahil/caelestia-connect | `f468e5371b5783e6665a7d391809fe981ba4f6bd` |

Los motores se distribuyen como submódulos con su código fuente completo y archivos
`LICENSES/`, `COPYING` y avisos SPDX. Las adaptaciones reproducibles están en
`scripts/engine/prepare.py`, `scripts/android-full/prepare.py`, `native/` y
`android/full/`; el árbol original permanece intacto. Obtén los submódulos con
`git submodule update --init --recursive` para reconstruir un APK/binario publicado.

Las traducciones españolas conservan sus autores; consulta `translations/README.md`.
El APK incorpora el aviso de dependencias de la compilación y los avisos originales de
sus bibliotecas. Qt y KDE Frameworks se enlazan dinámicamente; sus licencias se
encuentran en los paquetes de cada distribución. scrcpy y Android Platform Tools se
instalan de forma opcional, con sus licencias, separados del código propio.

La galería de https://plugins.omarchy.org/ se consultó como referencia de búsqueda,
filtros y tarjetas; no se instalaron plugins ni se incorporó su código. Los diseños
propios siguen la paleta Noctalia y las guías locales de diseño del usuario.
