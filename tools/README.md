# tools/

Build-time tooling for the telemetry CAN map.

| File | What it is |
|------|------------|
| `generate_914_dbc.py` | The authoritative form of the telemetry map. Emits `914-telemetry.dbc` on stdout and the firmware's `src/schema_id.h` via `--header`. |
| `914-telemetry.dbc` | Generated. The machine-readable map, for cantools / SavvyCAN / python-can. |

## Why this lives here

`src/schema_id.h` is compiled into the firmware. The generator used to live in the 914 notes
hub alongside the map write-up, which meant regenerating produced a header in one repository
that had to be hand-copied into this one. That copy step is precisely the drift the schema hash
exists to detect, so the generator now sits next to the code that consumes its output.

The prose description of the map -- the reasoning, the ID allocation table, the verification
record -- stays in the notes hub at `notes/can-id-allocation.md`. This script reads nothing at
runtime; the constants in it *are* the map. Change one, change the other.

## Regenerating

From the repo root:

    py -3.12 tools/generate_914_dbc.py --header=src/schema_id.h > tools/914-telemetry.dbc

The notes hub keeps its own copies, because `telemetry_encoding_check.py` and
`dual_emit_verify.py` load the DBC and the schema header from their own directory:

    py -3.12 tools/generate_914_dbc.py --header=<notes>/artifacts/schema_id.h \
        > <notes>/artifacts/914-telemetry.dbc

`<notes>` is machine-specific -- the hub is on a SeaDrive remote whose root differs per
computer. See `git-repo-paths.md` there.

Regenerate all of them together. A schema hash that differs between the header compiled into
the firmware and the DBC a decoder loaded is exactly the condition the broadcast is meant to
surface, and it is not worth discovering from a capture.

## Python

Use the executable recorded for this machine in the notes hub's
`python-executables-reference.md` rather than a bare `python`/`python3`, which on Windows
frequently resolves to the wrong version or the Microsoft Store redirect.
