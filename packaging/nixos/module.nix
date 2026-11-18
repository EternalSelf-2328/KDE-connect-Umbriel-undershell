{ self }:
{ config, lib, pkgs, ... }:
let cfg = config.services.umbriel-connect;
in {
  options.services.umbriel-connect = {
    enable = lib.mkEnableOption "Umbriel Connect's private phone integration";
    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      description = "Umbriel Connect package to use.";
    };
    autoStart = lib.mkEnableOption "starting the user services with the graphical session";
  };
  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];
    services.dbus.packages = [ cfg.package ];
    systemd.user.services.umbriel-connect-engine = {
      description = "Umbriel Connect private device engine";
      after = [ "graphical-session.target" ];
      partOf = [ "graphical-session.target" ];
      wantedBy = lib.optionals cfg.autoStart [ "graphical-session.target" ];
      serviceConfig = {
        Type = "dbus";
        BusName = "org.eternalself.UmbrielConnect.Engine";
        ExecStart = "${cfg.package}/bin/umbriel-connect-engine";
        Restart = "on-failure";
        RestartSec = 3;
        UMask = "0077";
      };
    };
    systemd.user.services.umbriel-connect-remote = {
      description = "Umbriel Connect authenticated control channel";
      after = [ "graphical-session.target" ];
      partOf = [ "graphical-session.target" ];
      wantedBy = lib.optionals cfg.autoStart [ "graphical-session.target" ];
      serviceConfig = {
        ExecStart = "${cfg.package}/bin/umbriel-connect-remote serve 127.0.0.1 31594";
        ExecStop = "${cfg.package}/bin/umbriel-connect-remote stop";
        Restart = "on-failure";
        RestartSec = 3;
        UMask = "0077";
      };
    };
  };
}
