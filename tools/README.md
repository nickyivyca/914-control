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

Two more copies live in the `reverse-it` workspace. No schema header is needed for either -- a
consumer checks the broadcast hash against the `SCHEMA_HASH=` stamp in the DBC's own `CM_` file
comment:

    py -3.12 tools/generate_914_dbc.py > <reverse-it>/projects/914/914-telemetry.dbc
    py -3.12 tools/generate_914_dbc.py \
        > <reverse-it>/android-app/CanLoggerApps/p914/src/main/assets/live-display/dbcs/vcu.dbc

**The second of those is the one the app actually loads at runtime.** `projects/914/` is a
reference copy in a stub project directory; the shipped asset is `vcu.dbc` under `:p914`. An
earlier version of this file said the app read `projects/914/914-telemetry.dbc`, which was never
true once the module was built -- corrected 2026-08-15.

**Four destinations, and they must be regenerated together.** A schema hash that differs between
the header compiled into the firmware and the DBC a decoder loaded is exactly the condition the
broadcast is meant to surface, and it is not worth discovering from a capture.

The failure mode is now worse than a stale decode, which is why the list above matters more than
it used to: since the DBC carries its own hash, a copy left un-regenerated makes the app report a
**false quarantine** -- it blanks the display and blames the firmware, when the only thing wrong
is that one of these four files was skipped. Check with `schema_stamp_verify.py` in the notes
hub, which compares the stamp against the header and can decode `SchemaId` out of a capture.

## Python

Use the executable recorded for this machine in the notes hub's
`python-executables-reference.md` rather than a bare `python`/`python3`, which on Windows
frequently resolves to the wrong version or the Microsoft Store redirect.
