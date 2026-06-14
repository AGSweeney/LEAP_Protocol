# Router DHCP for LeapOS NetBoot

The NetBoot server **does not run DHCP**. Configure your lab router (or a dedicated
DHCP server) to hand clients the TFTP server address and initial boot file.

## Required DHCP options

| Option | Name | Value |
|--------|------|--------|
| **66** | Boot Server Host Name / next-server | IPv4 of the NetBoot server |
| **67** | Bootfile Name | See below |

Clients also need normal network parameters (IP, mask, gateway, DNS) from the router.

## Recommended D945 Boot Filename

| Client | Option 67 | Notes |
|--------|-----------|--------|
| **D945GSEJT BIOS PXE** | `boot/grub/i386-pc/core.0` | Built into the server image; menu generated from Web UI state |

The current D945 lab uses GRUB PXE directly. iPXE is optional only; do not use it
unless you deliberately stage `ipxe.pxe`.

### Optional iPXE Second Stage

After `undionly.kpxe` / `ipxe.pxe` loads, iPXE performs a **second DHCP** request
(user-class `iPXE`). If your router supports per-class boot files, set the iPXE
class bootfile to:

```
ipxe/menu.ipxe
```

If the router cannot do that, use **GRUB PXE** (`boot/grub/i386-pc/core.0`) instead
— it needs no second-stage DHCP option.

## Example snippets

### pfSense / OPNsense

Services → DHCP Server → edit LAN → **Advanced** → add:

- **TFTP server**: `172.16.82.188`
- **Bootfile**: `boot/grub/i386-pc/core.0`

(Exact UI labels vary by version.)

### ISC dhcpd (if router runs isc-dhcp-server)

```conf
subnet 192.168.1.0 netmask 255.255.255.0 {
  # ... range, routers, dns ...
  next-server 172.16.82.188;
  filename "boot/grub/i386-pc/core.0";
}
```

### dnsmasq on router (DHCP only — not on NetBoot server)

```ini
dhcp-option=66,172.16.82.188
dhcp-option=67,boot/grub/i386-pc/core.0
```

## Firewall

Allow on the NetBoot server:

| Proto | Port | Purpose |
|-------|------|---------|
| UDP | 69 | TFTP |
| TCP | 80 | Web UI + HTTP boot |
| TCP | 8081 | API (localhost only; nginx proxies `/api/`) |

## Verification

1. Power a D945GSEJT test unit with **Legacy PXE** enabled.  
2. Watch serial COM1 @ 115200 — you should see GRUB, then the Alpine LeapOS Device payload.  
3. On the server: `tail -f /var/log/nginx/access.log` during boot.

If DHCP works but TFTP fails, check option **66** and that UDP/69 reaches the server.
