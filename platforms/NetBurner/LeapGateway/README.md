# LeapGateway — NetBurner

**LeapGateway-Embedded** ports for NetBurner modules (LEAP controller + EtherNet/IP
bridge + commissioning Web UI). Firmware builds in **NNDK** inside each target folder —
not in the repo-root CMake trees.

> **Naming:** Embedded gateway firmware uses **LeapGateway** / **LeapGateway-Embedded**.
> The **LeapOS-Gateway** name is reserved for the Linux appliance images (x86, Pi).

Shared application logic lives under
[`../../x86-32/D945GSEJT/LeapGateway/src/`](../../x86-32/D945GSEJT/LeapGateway/src/).
Use the active NetBurner **device** port for raw L2 transport patterns:

[`../MOD54415LC/`](../MOD54415LC/)

## Targets

| Module | Path | Status |
| --- | --- | --- |
| MOD54417 | [MOD54417/](MOD54417/) | planned — LeapGateway-Embedded |
