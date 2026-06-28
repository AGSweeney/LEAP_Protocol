# Rockwell Micro800 / Logix Run–Idle Header Compatibility

Rockwell originators (especially **Allen-Bradley Micro800** CCW/Connected Components Workbench, and some **Logix** I/O configurations) often behave inconsistently with the CIP **Run/Idle header** (4 bytes prepended to Class 1 O→T data):

- The Forward Open connection size may include **4 extra bytes** even when the connection path does **not** declare a Run/Idle header.
- Cyclic UDP packets may still carry a **4-byte Run/Idle field** while the declared assembly size matches **payload only**.

Stock OpENer (with `OPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER=ON`) then rejects Forward Open or writes assembly data at the wrong offset.

OpENer_uC-NetBurner addresses that mismatch with a three-part workaround, gated by **`OPENER_MICRO800_RUN_IDLE_COMPAT`** (CMake, **default ON**).

---

## Problem summary

| Symptom | Cause |
|---------|--------|
| Forward Open fails **Invalid O→T connection size** (+4 bytes) | Scanner connection size = assembly + seq + undeclared Run/Idle |
| I/O connects but output assembly is shifted / garbage | 4-byte Run/Idle prefix present on wire but not stripped |
| Identity stuck in idle extended status | Run/Idle dword not parsed when header mode is “off” |

Micro800 frequently uses **assembly-sized** connections on paper while still sending the **de-facto** 4-byte prefix on the wire.

---

## Implementation

### 1. Runtime: disable formal Run/Idle headers

Call from `ApplicationInitialization()` (or equivalent early init, before any Forward Open):

```c
CipRunIdleHeaderSetO2T(false);  /* do not expect declared O->T Run/Idle header */
CipRunIdleHeaderSetT2O(false);  /* do not emit T->O Run/Idle header */
```

This uses the runtime API added alongside compile-time `OPENER_*_RUN_IDLE_HEADER` defaults. Initial static defaults may still come from CMake, but the application **overrides** them before any Forward Open.

The demo application wraps this in `OpenerConfigureMicro800RunIdleCompat()` when compat mode is enabled.

### 2. Forward Open: implicit size compensation (O→T)

In `SetupIoConnectionOriginatorToTargetConnectionPoint()`, after subtracting the Class 1 sequence count:

```c
EipInt16 length_gap = data_size - assembly_length;
if ((length_gap == 4) && !s_consume_run_idle) {
  /* Micro800: connection size includes undeclared Run/Idle */
  data_size -= 4;
  diff_size += 4;
}
```

Accepts Forward Open when the scanner is exactly **4 bytes larger** than the assembly (the undeclared Run/Idle), only while `s_consume_run_idle == 0`.

### 3. Receive path: strip forced Run/Idle prefix

In `HandleReceivedIoConnectionData()`, when not in declared Run/Idle mode:

```c
if ((0 == s_consume_run_idle) &&
    (data_length == assembly_length + 4)) {
  data += 4;
  data_length -= 4;
}
```

Strips the de-facto prefix before `NotifyAssemblyConnectedDataReceived()`.

When `s_consume_run_idle == 1`, OpENer uses the standard path: parse the dword, update Identity extended status, call `RunIdleChanged()`, then subtract 4 bytes.

---

## Build integration

| Piece | Location |
|-------|----------|
| CMake option | `source/CMakeLists.txt` — `-DOPENER_MICRO800_RUN_IDLE_COMPAT=1` |
| Forward Open compensation | `source/src/cip/cipioconnection.c` — `SetupIoConnectionOriginatorToTargetConnectionPoint()` |
| Receive stripping | `source/src/cip/cipioconnection.c` — `HandleReceivedIoConnectionData()` |
| App hook | `OpenerConfigureMicro800RunIdleCompat()` — disables O→T/T→O headers |
| Demo app | `sample_application.c` calls `OpenerConfigureMicro800RunIdleCompat()` from `ApplicationInitialization()` |

### Configuration matrix

| Setting | Default | Effect |
|---------|---------------------|--------|
| `OPENER_MICRO800_RUN_IDLE_COMPAT` | **ON** | Enables compensation + stripping; `OpenerConfigureMicro800RunIdleCompat()` active |
| `OPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER` | ON | Compile-time default for `s_consume_run_idle`; app sets **false** when compat runs |
| `OPENER_PRODUCED_DATA_HAS_RUN_IDLE_HEADER` | OFF | No T→O Run/Idle header |

### Disable (strict CIP Run/Idle)

For scanners that correctly negotiate Run/Idle in the connection path:

```powershell
cmake ... -DOPENER_MICRO800_RUN_IDLE_COMPAT=OFF
```

Then set headers explicitly if needed:

```c
CipRunIdleHeaderSetO2T(true);   /* expect 4-byte O->T header */
CipRunIdleHeaderSetT2O(false);  /* or true if originator requires it */
```

Remove or guard the `OpenerConfigureMicro800RunIdleCompat()` call in your application.

### NBEclipse / firmware CPPFLAGS

When not using CMake for the app layer, add to match library build:

```
-DOPENER_MICRO800_RUN_IDLE_COMPAT=1
```

Rebuild **`libCIP.a`** after changing the option.

---

## Testing notes

1. **Micro800 CCW** — Exclusive Owner or Input-Only connection to demo assemblies 150→100.
2. Confirm Forward Open succeeds without editing assembly sizes in the PLC.
3. Wireshark UDP 2222 — O→T payload may show 4 bytes before assembly data; device should still copy correctly.
4. Toggle PLC Run/Idle — with compat mode, extended Identity status may not track Run/Idle dword (header is stripped, not parsed). Use `CheckIoConnectionEvent()` / application logic for run state if needed.

For full Run/Idle dword parsing, enable declared headers (`CipRunIdleHeaderSetO2T(true)`) **and** disable `OPENER_MICRO800_RUN_IDLE_COMPAT` — only when the originator declares the header correctly in Forward Open.

---

## Related API

```c
void OpenerConfigureMicro800RunIdleCompat(void);
void CipRunIdleHeaderSetO2T(bool onoff);
void CipRunIdleHeaderSetT2O(bool onoff);
bool CipRunIdleHeaderGetO2T(void);
bool CipRunIdleHeaderGetT2O(void);
void RunIdleChanged(EipUint32 run_idle_value);  /* app callback when header parsed */
```

---

## Related CMake options

- `OPENER_CONSUMED_DATA_HAS_RUN_IDLE_HEADER`
- `OPENER_PRODUCED_DATA_HAS_RUN_IDLE_HEADER`

When declared Run/Idle mode is enabled, the O→T prefix is a 32-bit field; bit 0 indicates Run vs Idle.
