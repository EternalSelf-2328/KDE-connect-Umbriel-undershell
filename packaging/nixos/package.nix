{ pkgs, src, engineSource }:
let
  inherit (pkgs) lib;
  runtimeTools = [ pkgs.wl-clipboard pkgs.openssh pkgs.sshfs ];
in pkgs.kdePackages.kdeconnect-kde.overrideAttrs (old: {
  pname = "umbriel-connect";
  version = "0.8.0";
  inherit src;
  patches = [];
  nativeBuildInputs = (old.nativeBuildInputs or []) ++ [
    pkgs.git pkgs.pkg-config (pkgs.python3.withPackages (p: [ p.cryptography ]))
  ];
  buildInputs = (old.buildInputs or []) ++ [
    pkgs.openssl pkgs.glib pkgs.libsysprof-capture pkgs.libpulseaudio pkgs.libwebp pkgs.kdePackages.prison
  ];
  postUnpack = ''
    mkdir -p "$sourceRoot/vendor/kdeconnect-kde"
    cp -r ${engineSource}/. "$sourceRoot/vendor/kdeconnect-kde/"
    chmod -R u+w "$sourceRoot/vendor/kdeconnect-kde"
  '';
  prePatch = "";
  postPatch = "";
  configurePhase = ''
    runHook preConfigure
    python3 scripts/engine/prepare.py
    cmake -S build/engine-source -B build/engine -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$out/lib/umbriel-connect/engine" \
      -DKDE_INSTALL_USE_QT_SYS_PATHS=OFF -DKDE_INSTALL_LIBDIR=lib \
      -DKDE_INSTALL_PLUGINDIR=lib/plugins -DKDE_INSTALL_QMLDIR=lib/qml \
      -DCMAKE_INSTALL_RPATH="$out/lib/umbriel-connect/engine/lib" \
      -DQtWaylandScanner_EXECUTABLE=${pkgs.qt6.qtbase}/libexec/qtwaylandscanner \
      -DINSTALL_UFW_APPLICATION_RULE=OFF -DBUILD_TESTING=ON
    cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$out" \
      -DCMAKE_INSTALL_LIBDIR=lib -DBUILD_TESTING=ON -DUMBRIEL_REQUIRE_DESKTOP_FEATURES=ON
    runHook postConfigure
  '';
  buildPhase = ''
    runHook preBuild
    cmake --build build/engine --parallel "$NIX_BUILD_CORES"
    cmake --build build/native --parallel "$NIX_BUILD_CORES"
    runHook postBuild
  '';
  doCheck = true;
  checkPhase = ''
    bash tests/private-session.sh ctest --test-dir build/engine --output-on-failure
    ctest --test-dir build/native --output-on-failure
  '';
  installPhase = ''
    runHook preInstall
    cmake --install build/engine --strip
    cmake --install build/native --strip
    mkdir -p "$out/share/licenses/umbriel-connect" "$out/share/doc/umbriel-connect"
    cp LICENSE "$out/share/licenses/umbriel-connect/"
    cp -r vendor/kdeconnect-kde/LICENSES "$out/share/licenses/umbriel-connect/engine"
    cp README.md THIRD_PARTY_NOTICES.md "$out/share/doc/umbriel-connect/"
    runHook postInstall
  '';
  dontWrapQtApps = true;
  postFixup = ''
    for program in "$out"/bin/* "$out"/lib/umbriel-connect/engine/bin/*; do
      if test -x "$program" && test -f "$program"; then
        wrapQtApp "$program" --prefix PATH : ${lib.makeBinPath runtimeTools}
      fi
    done
  '';
  meta = (old.meta or {}) // {
    description = "Phone integration for Umbriel, Noctalia and undershell with a private engine";
    homepage = "https://github.com/EternalSelf-2328/KDE-connect-Umbriel-undershell";
    mainProgram = "umbriel-connect";
    platforms = [ "x86_64-linux" ];
  };
})
