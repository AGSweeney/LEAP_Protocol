# LEAP Multi-Peer Notes

Reference-stack guidance for controlling multiple devices on one Ethernet
segment, and known gaps for true multi-controller coexistence.

## What the reference stack provides today

| Mechanism | Location | Purpose |
| --- | --- | --- |
| Per-peer `LeapControllerStack` slots | `leap_controller_session_hub` | Independent `session_id`, MGMT sequence, PD state, lease |
| Frame `sequence` tracking | `leap_controller_sequence` | Duplicate replay rejection, gap counting/rejection, optional window |
| Session binding | `leap_controller_stack` on OP entry | Drop inbound frames whose `session_id` ≠ owner session |
| PD endpoint validation | `leap_pd_profile_validate_*` | Profile ID, endpoint IDs, payload lengths from DIR map |
| Exchange reply validation | `leap_pd_validate_exchange_reply_at` | Endpoints, `process_sequence`, optional §13.4 frame age |
| Outbound PD timestamps | `leap_pd_controller` | Stamps `controller_timestamp_us` / `max_frame_age_us` on exchange |
| Foreign-owner skip | `leap_controller_session_hub` | `bootstrap_table` skips peers with another `active_owner_mac` |
| Linux recv demux | `leap_linux_pd.c` | `wait_exchange_reply` ignores frames from other peer MACs |
| I/O dirty flag | `LeapPdDeviceIoBinding.outputs_dirty` | Skip redundant output shadow writes |

## Configuration knobs

```c
LeapControllerStackConfig cfg;
cfg.frame_sequence.enforce_session_match = 1;  /* default when config NULL */
cfg.frame_sequence.window_size           = 64;
cfg.frame_sequence.reject_out_of_window  = 0;  /* set 1 for strict wire replay guard */
cfg.frame_sequence.reject_sequence_gaps  = 0;  /* set 1 to reject forward sequence gaps */

cfg.pd.validate_exchange_reply  = 1;  /* default when PD config NULL */
cfg.pd.enforce_reply_frame_age  = 1;  /* validate echoed timestamp on EXCHANGE_REPLY */
cfg.pd.max_frame_age_us         = 0;  /* 0 => 2 * cycle_period_ms */
cfg.pd.reply_jitter_margin_us   = 0;

LeapControllerSessionHubConfig hub_cfg;
hub_cfg.skip_foreign_owned_peers = 1;  /* default when hub config NULL */
memcpy(hub_cfg.default_peer.mgmt.controller_mac, local_mac, 6);
```

## Remaining risks (not implemented in v1 reference)

### Multi-controller arbitration / election

LEAP v1 assumes **one active owner** per device (MGMT owner lease). Two controllers
on the same wire without coordination will race for `OPEN_SESSION`; the device
enforces single ownership — this is not a symmetric multi-master fieldbus.

Partial mitigation today:

- Discovery stores `active_owner_mac` from HELLO_REPLY
- `leap_controller_peer_owned_by_other()` and hub `skip_foreign_owned_peers`
  avoid bootstrapping peers already owned by another MAC

Still out of scope for v1:

- Controller priority / token in MGMT extension
- Explicit owner election or redundant-controller failover profile
- Coordinated takeover when the incumbent controller fails

### Sequence + time validation

Implemented today:

- Ethernet `sequence` duplicate detection per peer (controller inbound)
- Optional strict gap rejection (`reject_sequence_gaps`)
- Device `process_sequence` enforcement (§13.6)
- Controller exchange-reply `process_sequence` check
- Category A frame age on inbound EXCHANGE_REPLY (§13.4 echoed timestamp)

Still weak / optional:

- Frame age on generic controller MGMT receive path (not only PD exchange reply)
- Bitmap replay window across reconnect (sequence baseline reset rules)

### Performance at scale

The Linux example uses a simulated I/O shadow with an `outputs_dirty` flag.
Embedded ports should:

- Mark endpoint groups dirty only when payload bytes change
- Avoid scanning full endpoint tables when a profile has many endpoints but
  only one group updates per cycle

## Recommended bring-up flow (multi-peer)

1. `leap_controller_peer_table_discover()` — collect MACs and `active_owner_mac`
2. Skip or defer peers where `leap_controller_peer_owned_by_other()` is true
3. `leap_controller_session_hub_bootstrap_peer()` per target (or `_bootstrap_table()`)
4. `leap_controller_session_hub_run_round_robin()` for cyclic PD
5. `leap_controller_session_hub_on_frame()` from a recv thread for async MGMT
6. `leap_controller_session_hub_release_all()` on shutdown

## See also

- Normative process sequence rules: `docs/LEAP_PROTOCOL_SPECIFICATION.md` §13.4–§13.6
- Controller stack plan: `docs/LEAP_CONTROLLER_STACK_PLAN.md`
