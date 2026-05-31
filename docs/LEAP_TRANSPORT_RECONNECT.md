# LEAP Transport — Link Monitoring and Reconnect Policy

Reference-stack guidance for Linux `AF_PACKET` examples and embedded ports.

Status as of May 2026. Auto-reconnect FSM is **not** implemented — policy only.

## Link state (`leap_raw_linux`)

| API | Purpose |
| --- | --- |
| `leap_raw_linux_query_link()` | Snapshot `IFF_UP`, `IFF_RUNNING`, derived `link_up` |
| `leap_raw_linux_poll_link()` | Detect transitions vs last poll; increments `stats.link_transitions` |
| `leap_linux_poll_link_and_log()` | Example helper — prints on transition |

`link_up` means administratively up **and** carrier running (`IFF_UP && IFF_RUNNING`).
On virtual interfaces (`lo`, veth) both are typically set when the interface is up.

The device example polls link each recv loop iteration. Controllers blocked in
`run_cyclic_pd` / hub round-robin should poll from a side thread or between
bootstrap and cyclic if link monitoring is required during steady-state PD.

## Reconnect policy (reference stack)

LEAP v1 does not define automatic session recovery in the wire protocol. The
reference stack uses explicit application policy:

| Event | Recommended action |
| --- | --- |
| **Link down** (carrier lost) | Stop PD immediately; do not send heartbeats into a dead segment. Release owner sessions when practical (`leap_controller_stack_release` / hub `release_all`). |
| **Link up** (carrier restored) | Full **rediscover + rebootstrap**. Do not assume prior `session_id` or Ethernet `sequence` baselines remain valid. |
| **Controller process restart** | Rediscover; `OPEN_SESSION` with new owner lease. Device may still show prior owner in HELLO until lease expires. |
| **Single peer, same MAC** | `leap_controller_stack_bootstrap_peer()` or full `bootstrap()` after transport reopen. |
| **Multi-peer hub** | `leap_controller_session_hub_release_all()` → `peer_table_discover()` → `bootstrap_table()`. |
| **Recv timeout storm** (PD) | Treat as comms loss for that peer; release slot and optionally retry bootstrap after link_up. |
| **Foreign owner at discover** | Skip bootstrap (`skip_foreign_owned_peers`) or wait for incumbent lease expiry. |

### What not to do

- Resume cyclic PD on the old `session_id` after long link outage without MGMT validation.
- Share one `LeapControllerStack` across peers without the session hub.
- Ignore `active_owner_mac` from HELLO when another controller may be online.

### Device side

Devices remain in last known MGMT state until lease/watchdog expiry (`leap_device_stack_tick`).
Link loss on the device does not automatically drop ownership — comms timeout drives SAFE.

## Example integration points

```c
/* Device recv loop (see device_main.c) */
leap_linux_poll_link_and_log(&transport);

/* After link-up following outage */
leap_controller_stack_reset(&stack);
(void)leap_controller_stack_bootstrap(&stack, &io, peer_mac);

/* Hub multi-peer */
leap_controller_session_hub_release_all(&hub, &io);
leap_controller_peer_table_init(&table);
(void)leap_controller_peer_table_discover(&table, &io, scan_ms);
(void)leap_controller_session_hub_bootstrap_table(&hub, &io, &table, &n);
```

## See also

- [LEAP_MULTI_PEER_NOTES.md](LEAP_MULTI_PEER_NOTES.md) — multi-peer failure modes
- [examples/linux_loopback/README.md](../examples/linux_loopback/README.md) — wire examples
- Normative comms recovery: `LEAP_PROTOCOL_SPECIFICATION.md` (MGMT lease / rediscover)
