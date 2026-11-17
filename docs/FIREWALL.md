# Descubrimiento local y firewall

Un receptor escuchando no prueba que el firewall permita tráfico desde el teléfono.
Las pruebas contra la propia dirección de la PC pueden circular por loopback, que
UFW acepta antes de las reglas de entrada. Tampoco la ausencia de logs demuestra
que el broadcast esté permitido: UFW puede omitir el registro de esos descartes.

| Función | Entrada necesaria en la PC |
|---|---|
| Encontrar la computadora desde Inicio de Android | UDP31595 |
| Vincular por código, controles y archivos propios | TCP31594 |
| Funciones del motor compatible | TCP/UDP1714–1764 |
| mDNS del motor | UDP5353 multicast, según la red y el firewall |

El permiso antiguo para KDE Connect solo cubre1714–1764: no cubre los puertos propios
31594/31595. Debe revisarse aunque el motor compatible estuviera permitido.

## UFW

Comprueba interfaz y prefijo reales; no uses una subred de ejemplo sin adaptarla:

```bash
ip -4 address show
sudo ufw status verbose
ss -lntup
```

Ejemplo para una LAN `10.42.0.0/24` por `enp1s0`:

```bash
sudo ufw allow in on enp1s0 proto udp from 10.42.0.0/24 to any port 31595 comment 'Umbriel Connect discovery LAN'
sudo ufw allow in on enp1s0 proto tcp from 10.42.0.0/24 to any port 31594 comment 'Umbriel Connect TLS LAN'
sudo ufw status numbered
```

Se usa destino `any` para admitir también las consultas broadcast. La interfaz y
el origen limitan el acceso a esa LAN. No hace falta desactivar/reiniciar UFW,
abrir puertos en el router, modificar DNS/rutas ni cambiar Tailscale. No retires
otras reglas para probar: podrían sostener tu conexión remota.
[Sintaxis oficial de UFW](https://manpages.ubuntu.com/manpages/noble/man8/ufw.8.html).

Alternativa con los perfiles opcionales incluidos en el repositorio:

```bash
sudo install -m644 data/ufw-umbriel-connect /etc/ufw/applications.d/umbriel-connect
sudo ufw allow in on enp1s0 from 10.42.0.0/24 to any app 'Umbriel Connect'
# Solo si necesitas también el motor compatible y no está permitido:
sudo ufw allow in on enp1s0 from 10.42.0.0/24 to any app 'Umbriel Connect Features'
```

El instalador de Umbriel no altera el firewall automáticamente. El perfil es una
alternativa a las reglas por puerto: no hace falta aplicar ambas formas.

## Verificar desde el teléfono

Abre Inicio de Umbriel Connect en Android, pulsa Buscar mi computadora y solicita
el vínculo por código. La solicitud debe aparecer en Inicio de la PC. Si no aparece,
comprueba versión instalada, permisos Android, misma LAN y aislamiento de clientes
en el punto de acceso. No borres los datos del teléfono para probar la detección.

Después de cambiar interfaz o subred, revisa las reglas: una regla limitada a la
LAN anterior seguirá limitada a ella. Tailscale userspace/Serve conserva su recorrido
independiente; estas reglas no lo configuran ni lo sustituyen.

Para revertir el ejemplo, elimina solo las dos reglas creadas:

```bash
sudo ufw delete allow in on enp1s0 proto udp from 10.42.0.0/24 to any port 31595
sudo ufw delete allow in on enp1s0 proto tcp from 10.42.0.0/24 to any port 31594
```
