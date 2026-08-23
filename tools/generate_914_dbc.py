"""
Generate 914-telemetry.dbc and src/schema_id.h -- the authoritative form of the telemetry map.

The map is transcribed here from the write-up in the 914 notes hub (notes/can-id-allocation.md).
Nothing is read at runtime: the constants below are the map, and this is the copy the firmware
and every decoder actually follow. It lives in this repo rather than beside the notes because
src/schema_id.h is compiled into the firmware -- generating it from another repository meant a
manual copy step between the two, which is exactly the drift the schema hash exists to catch.

Generated rather than hand-written because the map contains 168 cell-voltage signals and 168
balancing bits, which is more than can be typed correctly, and because the pack layout has
already changed five times across the log archive (94, 101, 102, 103, 104 and now 206 CSV
columns). When it changes again, edit the constants here and regenerate.

Byte order is little-endian (Intel, @1) throughout -- the LPC1768 is little-endian and mixing
orders is a reliable source of silent decode errors.

Also emits a schema identity that the firmware broadcasts, so a consumer can tell whether the
stream it is decoding matches the DBC it has loaded. The hash is computed here from the map
itself and written to a C header, which is what keeps the two from drifting: change the map,
regenerate, and the broadcast value changes with it. Hand-maintaining a version number is the
failure mode this exists to prevent.

Regenerate both consumers from this one source; run from the repo root:

    py -3.12 tools/generate_914_dbc.py --header=src/schema_id.h > tools/914-telemetry.dbc

and refresh the notes hub's analysis copies, which telemetry_encoding_check.py and
dual_emit_verify.py read from their own directory:

    py -3.12 tools/generate_914_dbc.py --header=<notes>/artifacts/schema_id.h \
        > <notes>/artifacts/914-telemetry.dbc

The notes-hub path is machine-specific (SeaDrive); see git-repo-paths.md there.

ASCII-only output, per the project rules (Windows console is cp1252).
"""

import hashlib
import sys

# Bump MAJOR when an existing signal moves, changes scaling, or a message id changes -- an old
# consumer would decode wrongly. Bump MINOR when signals or messages are only added -- an old
# consumer decodes everything it knows and simply misses the new data.
# 4.1 (2026-08-22): the charger's own RX map transcribed -- Tesla module state, DC and
# temperatures, the two signals missing from the AC frames, and the VCU's two command frames.
# MINOR: every existing signal decodes exactly as before, an old consumer just misses the new
# messages.
# 4.0 (2026-08-18): InvIq scaling negated, so the torque-producing current reads positive when
# the car drives forward. MAJOR, not MINOR, and the rule above is why -- an old consumer decoding
# with factor +0.1 gets a plausible, correctly-scaled, wrong-signed number. That is precisely the
# "plausible wrong numbers rather than an error" case the SchemaId comment reserves quarantine for.
SCHEMA_MAJOR = 4
SCHEMA_MINOR = 1

NUM_CELLS = 168          # NUM_CHIPS * NUM_CELLS_PER_CHIP
CELLS_IN_SERIES = 84     # NUM_CHIPS * NUM_CELLS_PER_CHIP / NUM_STRINGS -- two parallel strings
NUM_THERMISTORS = 12
NUM_DIE_TEMPS = 12
CELLS_PER_BOX = 28       # display label is 'A' + index/28

# MCP23017 input pins, port B. Not read by the mainline firmware; the knob switches are on the
# charge-control branch only. The map reserves the byte anyway so that branch can land without
# renumbering the status frame.
MCP_PIN_KNOB1SW = 11
MCP_PIN_KNOB2SW = 12

EXT = 0x80000000         # DBC marks an extended (29-bit) id with the top bit

# Synthetic telemetry, all extended ids.
ID_STATUS      = 0x1F000000
ID_PACK        = 0x1F000001
ID_CELL_SUM    = 0x1F000002
ID_TEMP_SUM    = 0x1F000003
ID_CELL_BASE   = 0x1F000010
ID_THERM_BASE  = 0x1F000040
ID_DIE_BASE    = 0x1F000050
ID_LINK        = 0x1F000060
ID_BAL_BASE    = 0x1F000070
ID_DIAG        = 0x1F0000F0
ID_SCHEMA      = 0x1FFFFFFE
ID_INTEGRITY   = 0x1FFFFFFF

# Real vehicle bus, standard 11-bit ids.
ID_INVERTER    = 0x001
ID_INVERTER_ST = 0x002
ID_INVERTER_FOC = 0x003
ID_INVERTER_THR = 0x004
ID_VCU_CONTROL = 0x03F
ID_CHARGER_AC  = (0x207, 0x209, 0x20B)

# The rest of what the Tesla modules broadcast. The charger controller receives all of this and
# uses it internally -- CalcTotals() sums Idc and takes the highest Udc, CheckChargerFaults()
# watches Stt and Flag -- but none of it was transmitted onward, so it sat on the bus undescribed.
# Transcribed from the AddRecv calls in stm32-teslacharger/src/chargercan.cpp; every position,
# length, gain and offset below is copied from there rather than inferred from a capture.
#
# These frames only exist while AC is connected: the modules are unpowered otherwise.
#
# Not included: 0x247/0x249/0x24B (cNtmplim). Those AddRecv lines are commented out in the
# charger ("We don't have enough space for all messages"), so the charger does not receive them
# and they are not part of what the source defines. They are also self-inconsistent as written --
# the call passes gain 7 while the trailing comment says 0.234375 -- so an entry here would be a
# guess rather than a transcription.
ID_CHARGER_STT = (0x217, 0x219, 0x21B)
ID_CHARGER_DC  = (0x227, 0x229, 0x22B)
ID_CHARGER_TMP = (0x237, 0x239, 0x23B)

# openinverter's SDO server, the only route to parameters the CAN map cannot carry. nodeid is 1
# on this car, so 0x600+1 for the request and 0x580+1 for the reply -- the same pair CanSdo
# registers as a user message filter. See notes/inverter-sdo-polling.md.
INVERTER_NODE_ID = 1
ID_INVERTER_SDO_REQ  = 0x600 + INVERTER_NODE_ID
ID_INVERTER_SDO_RESP = 0x580 + INVERTER_NODE_ID

# Frames transmitted by our own charger controller (stm32-teslacharger), taken from the AddSend
# calls in its src/chargercan.cpp. NOT the Tesla modules -- those speak 0x207/0x209/0x20B and are
# unpowered without AC, whereas the controller transmits these seven at 10 Hz whenever the car is
# on. Every field below was checked against observed payloads in serial-slcan-realistic.log.gz.
#
# openinverter's CanMap transmits raw = physical * gain + offset, so a DBC factor is 1/gain and a
# DBC offset is -offset/gain. Several fields abuse `Param::version` with gain 0 purely to emit a
# constant byte; those are declared here as the constants they produce, because that is what is
# actually on the wire.
ID_CHGCTL_CHADEMO_LIMITS = 0x108
ID_CHGCTL_CHADEMO_STATUS = 0x109
ID_CHGCTL_IDENT          = 0x368
ID_CHGCTL_MODULE         = (0x42C, 0x43C, 0x44C)
ID_CHGCTL_COMMAND        = 0x45C

# The other direction: what the VCU sends the charger. 0x102 is upstream's CHAdeMO RX map, reused;
# 0x103 is ours, added for the dash termination and module knobs. Both are transcribed from the
# AddRecv calls, same as the module frames above.
ID_CHGCMD_CHADEMO  = 0x102
ID_CHGCMD_SETPOINT = 0x103

out = []
_allocated = {}

# Canonical record of the map's *structure*, used for the schema hash. Deliberately excludes
# comments, value tables, min/max and units: rewording a comment must not invalidate every
# consumer's cached schema. Only things that change how a byte decodes are in here.
_schema = []


def emit(s=""):
    out.append(s)


def msg(ident, name, dlc, tx="VCU", extended=True):
    # Guard against two blocks growing into each other. The block bases below are fixed
    # constants while the frame counts are derived from NUM_CELLS and friends, so raising a
    # count silently walks one block into the next -- the cell block has only 6 spare frames,
    # 24 cells, before it reaches the thermistors at 0x1F000040. Without this the collision
    # would appear as a duplicate BO_ that some tools accept and others reject, decoding cell
    # voltages as temperatures.
    if ident in _allocated:
        raise SystemExit(
            "ID collision: 0x%08X wanted by %s but already used by %s.\n"
            "A block has outgrown its range -- move a base constant, do not renumber by hand."
            % (ident, name, _allocated[ident]))
    _allocated[ident] = name
    _schema.append("M|%08X|%d|%s|%d" % (ident, 1 if extended else 0, name, dlc))
    emit("BO_ %d %s: %d %s" % ((ident | EXT) if extended else ident, name, dlc, tx))


def sig(name, start, length, signed=False, factor=1, offset=0,
        lo=None, hi=None, unit="", rx="LOGGER"):
    if lo is None:
        lo = 0
    if hi is None:
        hi = (1 << length) - 1 if not signed else (1 << (length - 1)) - 1
    _schema.append("S|%s|%d|%d|%d|%s|%s"
                   % (name, start, length, 1 if signed else 0, factor, offset))
    emit(' SG_ %s : %d|%d@1%s (%s,%s) [%s|%s] "%s" %s'
         % (name, start, length, "-" if signed else "+", factor, offset, lo, hi, unit, rx))


def comment_msg(ident, text, extended=True):
    emit('CM_ BO_ %d "%s";' % ((ident | EXT) if extended else ident, text))


def comment_sig(ident, name, text, extended=True):
    emit('CM_ SG_ %d %s "%s";' % ((ident | EXT) if extended else ident, name, text))


# ---------------------------------------------------------------- header

emit('VERSION "914 EV telemetry -- generated by generate_914_dbc.py, do not hand-edit"')
emit()
emit()
emit("NS_ :")
for tag in ("NS_DESC_", "CM_", "BA_DEF_", "BA_", "VAL_", "CAT_DEF_", "CAT_", "FILTER",
            "BA_DEF_DEF_", "EV_DATA_", "ENVVAR_DATA_", "SGTYPE_", "SGTYPE_VAL_",
            "BA_DEF_SGTYPE_", "BA_SGTYPE_", "SIG_TYPE_REF_", "VAL_TABLE_", "SIG_GROUP_",
            "SIG_VALTYPE_", "SIGTYPE_VALTYPE_", "BO_TX_BU_", "BA_DEF_REL_", "BA_REL_",
            "BA_DEF_DEF_REL_", "BU_SG_REL_", "BU_EV_REL_", "BU_BO_REL_", "SG_MUL_VAL_"):
    emit("\t" + tag)
emit()
emit("BS_:")
emit()
emit("BU_: VCU INVERTER CHARGER LOGGER")
emit()

# ---------------------------------------------------------------- fast tier, 10 Hz

msg(ID_STATUS, "BmsStatus", 8)
FAULTS = ("CellOverVoltage", "CellUnderVoltage", "OverTemp", "StringImbalance", "Pec")
for i, f in enumerate(FAULTS):
    sig("Flt_%s_Latched" % f, i, 1)
for i, f in enumerate(FAULTS):
    sig("Flt_%s_Now" % f, 8 + i, 1)
STATUS = ("Discharging", "ChargeSwitch", "ChargeEnable", "Contactors",
          "Balancing", "SocReserve", "VoltageCheckOk", "StringCheckOk")
for i, s in enumerate(STATUS):
    sig("Sts_%s" % s, 16 + i, 1)
# Byte 3 mirrors the literal value handed to MCP23017::write_mask(). Only the bits in
# MCP_BMS_THREAD_MASK are owned by the BMS thread.
for name, bit in (("G", 0), ("LowFuel", 3), ("Egr", 4), ("BmsErr", 7)):
    sig("Ioexp_%s" % name, 24 + bit, 1)
sig("IoexpMask", 32, 8)
sig("ErrCount", 40, 16)
# Byte 7: MCP23017 port B, the input side. Reserved in the map before anything on the mainline
# branch reads it -- the knob switches exist only on the charge-control branch today. Holding
# the byte now means that branch does not have to renumber the frame when it lands.
#
# Emitted as eight named 1-bit signals rather than one byte plus overlays: a DBC may not have
# overlapping signals, and cantools rejects the file outright if it does. Naming every bit
# documents the whole reservation and makes the two known switches decodable the moment they
# are implemented.
GPIO_IN_NAMES = {MCP_PIN_KNOB1SW: "Knob1Sw", MCP_PIN_KNOB2SW: "Knob2Sw"}
for pin in range(8, 16):
    sig("Gpi_%s" % GPIO_IN_NAMES.get(pin, "Pin%02d" % pin), 56 + (pin - 8), 1)
emit()

msg(ID_PACK, "BmsPack", 8)
sig("PackVoltage", 0, 16, factor=0.1, hi=6553.5, unit="V")
sig("PackCurrent", 16, 16, signed=True, factor=0.01, lo=-327.68, hi=327.67, unit="A")
sig("Soc", 32, 8, hi=100, unit="%")
sig("NumBalancing", 40, 8, hi=NUM_CELLS)
sig("Energy", 48, 16, factor=0.01, hi=655.35, unit="kWh")
emit()

msg(ID_CELL_SUM, "BmsCellSummary", 8)
sig("CellMin", 0, 16, unit="mV")
sig("CellMinIndex", 16, 8, hi=NUM_CELLS - 1)
sig("CellMax", 24, 16, unit="mV")
sig("CellMaxIndex", 40, 8, hi=NUM_CELLS - 1)
sig("CellAvg", 48, 16, unit="mV")
emit()

msg(ID_TEMP_SUM, "BmsTempSummary", 8)
sig("TempMin", 0, 16, signed=True, factor=0.1, lo=-3276.8, hi=3276.7, unit="degC")
sig("TempMinBox", 16, 8, hi=NUM_THERMISTORS - 1)
sig("TempMax", 24, 16, signed=True, factor=0.1, lo=-3276.8, hi=3276.7, unit="degC")
sig("TempMaxBox", 40, 8, hi=NUM_THERMISTORS - 1)
sig("InverterHeatsink", 48, 16, signed=True, factor=0.1, lo=-3276.8, hi=3276.7, unit="degC")
emit()

# ---------------------------------------------------------------- slow tier, 2 Hz

for frame in range((NUM_CELLS + 3) // 4):
    msg(ID_CELL_BASE + frame, "BmsCellVoltages_%02d" % frame, 8)
    for k in range(4):
        cell = frame * 4 + k
        if cell < NUM_CELLS:
            sig("Cell_%03d" % cell, k * 16, 16, hi=65535, unit="mV")
    emit()

for frame in range((NUM_THERMISTORS + 3) // 4):
    msg(ID_THERM_BASE + frame, "BmsTemps_%d" % frame, 8)
    for k in range(4):
        t = frame * 4 + k
        if t < NUM_THERMISTORS:
            sig("Temp_%02d" % t, k * 16, 16, signed=True, factor=0.1,
                lo=-3276.8, hi=3276.7, unit="degC")
    emit()

for frame in range((NUM_DIE_TEMPS + 7) // 8):
    msg(ID_DIE_BASE + frame, "BmsDieTemps_%d" % frame, 8)
    for k in range(8):
        d = frame * 8 + k
        if d < NUM_DIE_TEMPS:
            sig("DieTemp_%02d" % d, k * 8, 8, signed=True, lo=-128, hi=127, unit="degC")
    emit()

msg(ID_LINK, "BmsLinkHealth", 8)
sig("CanSwDrops", 0, 16)
sig("CanHwOverruns", 16, 16)
sig("CanQueuePeak", 32, 16)
sig("CanMaxLatency", 48, 16, unit="us")
emit()

BAL_FRAMES = (NUM_CELLS + 63) // 64
for frame in range(BAL_FRAMES):
    msg(ID_BAL_BASE + frame, "BmsBalanceMask_%d" % frame, 8)
    for bit in range(64):
        cell = frame * 64 + bit
        if cell < NUM_CELLS:
            sig("Bal_Cell_%03d" % cell, bit, 1)
    emit()

# ---------------------------------------------------------------- diagnostics + integrity

msg(ID_DIAG, "BmsDiagnostic", 8)
sig("DiagCode", 0, 8)
sig("DiagArg1", 8, 16)
sig("DiagArg2", 24, 16)
sig("DiagArg3", 40, 16)
emit()

msg(ID_SCHEMA, "SchemaId", 8)
sig("SchemaMajor", 0, 8)
sig("SchemaMinor", 8, 8)
sig("SchemaHash", 16, 32)
emit()

msg(ID_INTEGRITY, "SlcanIntegrity", 8)
sig("SeqCounter", 0, 16)
sig("FrameCount", 16, 16)
sig("Crc16", 32, 16)
emit()

# ---------------------------------------------------------------- real vehicle bus

msg(ID_INVERTER, "InverterStatus", 8, tx="INVERTER", extended=False)
# Bytes 0-1 were unmapped in the inverter until 2026-08-10 and therefore always read zero, which
# is why the scale sat "unconfirmed" for so long -- there was nothing there to confirm. `udc` is
# now mapped at gain 10, so the scale is defined rather than inferred. Bytes 6-7 remain free.
sig("InvDcVoltage", 0, 16, factor=0.1, hi=6553.5, unit="V")
sig("InvHeatsinkTemp", 16, 16, signed=True, factor=0.1, lo=-3276.8, hi=3276.7, unit="degC")
sig("InvRpm", 32, 16, unit="rpm")
emit()

# Inverter state. NOT transmitted yet -- this frame does not exist on the bus until four
# `can tx` entries are added to the inverter's map (see can-id-allocation.md). Declared now for
# the same reason as the GPIO input byte: so the map does not have to be renumbered later, and
# so the decode is ready the moment the entries are added.
#
# A second frame rather than the spare bytes of 0x001, for two reasons. `status` is 9 bits and
# only one byte of 0x001 is free, and those "free" bytes read zero in a capture taken with the
# contactors open -- which is exactly what `udc` does too, so zero does not prove unmapped.
# 0x002 is unused on this bus and carries no such doubt.
#
# Bit meanings are openinverter's own, from STATUS and CANIOS in stm32-sine/param_prj.h. They
# are declared as individual bits rather than a VAL_ table because both are bitfields: several
# can be set at once, and a value table would decode 0x11 as an unknown enum rather than as
# UdcLow plus EmcyStop.
msg(ID_INVERTER_ST, "InverterState", 8, tx="INVERTER", extended=False)
# STATUS is 10 bits, not 9. BrakeCheck was missing here until 2026-08-13, and the car sits at
# status = 516 = BrakeCheck | UdcBelowUdcSw at rest -- so the bit that was being dropped is set
# in the vehicle's normal resting state, not some rare condition.
INV_STATUS = ("UdcLow", "UdcHigh", "UdcBelowUdcSw", "UdcLim", "EmcyStop",
              "MProt", "PotPressed", "TmpHs", "WaitStart", "BrakeCheck")
for i, s in enumerate(INV_STATUS):
    sig("InvSts_%s" % s, i, 1)
sig("InvOpmode", 16, 8, hi=6)
# Bits 24-29 carry the six din_* spot values, NOT canio -- canio is on the boot-time removal
# list (see "Why five values cannot be mapped") and cannot be mapped at all. The six din_* carry
# the same six inputs in the same order, so the signal names did not have to change, but the
# MEANING did: each din_* is the OR of the physical pin and the canio bit
# (vehiclecontrol.cpp:392-399), so this is the inverter's *effective* input rather than only
# what arrived over CAN. InvIo_Brake and InvIo_Bms are still faithful echoes of 0x03F because
# those two physical pins read 0 on this car; InvIo_Fwd sits high on a real wire and echoes
# nothing.
INV_CANIO = ("Cruise", "Start", "Brake", "Fwd", "Rev", "Bms")
for i, s in enumerate(INV_CANIO):
    sig("InvIo_%s" % s, 24 + i, 1)
sig("InvAuxVoltage", 32, 8, factor=0.1, hi=25.5, unit="V")
# Two bits each, because these are tri-state: 0=Error, 1=Ok, 2=na, where `na` is what a board
# that does not wire them reports. hwver is MiniMainboard here, which does wire them.
sig("InvIo_Ocur", 40, 2, hi=2)
sig("InvIo_Desat", 42, 2, hi=2)
# Raw pin states, deliberately distinct from the latched InvSts_MProt / InvSts_EmcyStop above.
sig("InvIo_MProt", 44, 1)
sig("InvIo_EmcyStop", 45, 1)
emit()

# Also not transmitted yet. Signed fields are declared signed here with a POSITIVE length in the
# map: openinverter's CanMap does `int ival = physical*gain + offset; ival &= (1<<numBits)-1`, so
# a negative value lands as two's complement in the field and a signed DBC signal reads it back.
# A *negative* length in the map means big-endian, not signed -- and is unusable on the send
# side anyway, because that same masking line shifts by a negative count.
msg(ID_INVERTER_FOC, "InverterFoc", 8, tx="INVERTER", extended=False)
sig("InvId", 0, 16, signed=True, factor=0.1, lo=-3276.8, hi=3276.7, unit="A")
# NEGATIVE factor. The motor is installed turning the opposite way to the inverter's own
# convention, so the torque-producing current reads negative when the car drives forward. The sign
# is corrected here, once, rather than in every consumer: a log, a cantools session and the
# display would otherwise each have to remember to flip it, and one of them eventually would not.
#
# id is deliberately NOT flipped. Reversing rotation inverts the torque axis, not the flux axis --
# borne out by the 2026-08-18 drive log, where id sits symmetrically about zero (-74.4 .. +50.6 A)
# with no directional bias while iq is one-sided.
#
# ifw is NOT flipped either, and must not be: field weakening is negative d-axis current by
# convention, not because of this car. stm32-sine defines `fwcurmax` over [-1000, 0] with a
# default of -100 (`param_prj.h:58`), so ifw can only ever be <= 0 and a positive reading would be
# meaningless. The display negates it for plotting and labels that axis "-ifw"; the wire value
# stays honest.
sig("InvIq", 16, 16, signed=True, factor=-0.1, lo=-3276.8, hi=3276.7, unit="A")
sig("InvIfw", 32, 16, signed=True, factor=0.1, lo=-3276.8, hi=3276.7, unit="A")
sig("InvAmp", 48, 16, unit="dig")
emit()

msg(ID_INVERTER_THR, "InverterThrottle", 8, tx="INVERTER", extended=False)
# Bytes 0-3 (was InvPot / InvPot2) and byte 5 (was InvRegenPreset) are FREE as of schema 3.0.
#
# Those three are on the boot-time removal list in UpgradeParameters(), so the inverter deletes
# their map entries on every power cycle and the bytes can only ever read a constant zero. A
# consumer decoding them would report a permanently released throttle and zero regen preset --
# plausible, wrong, and indistinguishable from a real reading. Declaring nothing is strictly
# better than declaring something that always lies. Confirmed on the car: the four entries were
# re-added, saved (both "CANMAP stored" and "Parameters stored"), power cycled, and were gone.
#
# The raw ADC digits are not lost, only de-periodised: poll pot (id 2015) and pot2 (id 2016)
# over SDO on 0x601/0x581. That is what InverterSdoReq/InverterSdoResp below are for.
#
# This removal is why schema 3.0 is a MAJOR bump. Nothing needs quarantining -- the signals only
# ever carried zero -- but the rule is applied mechanically rather than argued away.
#
# The resolved throttle command, signed because regen is negative. throtmin/throtmax are -100
# and 100, so an int8 covers the full range exactly. This one survives, and it is the throttle
# value that actually reflects what the inverter acted on.
sig("InvPotNom", 32, 8, signed=True, lo=-100, hi=100, unit="%")
# Byte 6 was going to be `dir`, but this firmware rejects that name -- it is not a spot value in
# the build the car runs, whatever the newer param_prj.h snapshot says. Nothing is lost: the
# commanded direction is already in 0x002 as InvIo_Fwd and InvIo_Rev, and the sign of InvPotNom
# gives drive-versus-regen. Byte 6 left free.
sig("InvCpuLoad", 56, 8, hi=100, unit="%")
emit()

msg(ID_VCU_CONTROL, "VcuControl", 8, tx="VCU", extended=False)
sig("BmsLimit", 29, 1)
emit()

# ------------------------------------------------------------------ inverter SDO
#
# CANopen-style expedited request/response, the only route to parameters the periodic CAN map
# cannot carry. Both frames are declared with their raw fields and paired in post-processing:
# SDO is not periodic, and the 24-bit index/subIndex pair that selects the meaning is not
# expressible as a DBC multiplexer.
#
# SdoData is declared SIGNED because a parameter read returns Param::Get(), which is s32fp --
# fixed point with 5 fractional bits, i.e. the real value times 32 -- and regen values are
# negative. The 1/32 scale is deliberately NOT applied as a DBC factor here: the same field
# carries non-parameter payloads for other indices (CAN map records, string transfers, abort
# codes), so scaling it globally would corrupt those. Apply the /32 in the consumer, keyed on
# index/subIndex, and use an arithmetic shift or a signed divide -- >> 5 on an unsigned type
# decodes every regen reading as roughly +134 million.
for _ident, _name, _tx, _rx in ((ID_INVERTER_SDO_REQ, "InverterSdoReq", "VCU", "INVERTER"),
                                (ID_INVERTER_SDO_RESP, "InverterSdoResp", "INVERTER", "LOGGER")):
    msg(_ident, _name, 8, tx=_tx, extended=False)
    sig("SdoCmd", 0, 8, rx=_rx)
    sig("SdoIndex", 8, 16, rx=_rx)
    sig("SdoSubIndex", 24, 8, rx=_rx)
    sig("SdoData", 32, 32, signed=True, lo=-2147483648, hi=2147483647, rx=_rx)
    emit()

# Gain 0.06666f in the charger source for both AC currents. Written as 1/15 here, which is what
# that constant approximates and what the existing Iac entry already used; keeping the two
# consistent matters more than the fifth decimal, and a mixed pair inside one message would look
# like a real difference.
_AC_GAIN = 1.0 / 15.0

for n, ident in enumerate(ID_CHARGER_AC, start=1):
    msg(ident, "Charger%dAc" % n, 8, tx="CHARGER", extended=False)
    sig("Uac", 8, 8, unit="V")
    # Two bits only. The charger's CHFLAGS enum names a third value, 4=CheckAlive, which does not
    # fit the mapped width -- so whatever the modules report there, the charger cannot see it.
    sig("Flag", 17, 2, hi=3)
    # All three AC frames carry this and the charger maps all three onto the same parameter, so
    # the last frame to arrive wins. Present per-module here because that is what is on the wire.
    sig("HwAcLim", 32, 9, factor=_AC_GAIN, hi=34.1, unit="A")
    sig("Iac", 41, 9, factor=_AC_GAIN, hi=34.1, unit="A")
    emit()

for n, ident in enumerate(ID_CHARGER_STT, start=1):
    msg(ident, "Charger%dStatus" % n, 8, tx="CHARGER", extended=False)
    sig("Stt", 0, 8)
    emit()

for n, ident in enumerate(ID_CHARGER_DC, start=1):
    msg(ident, "Charger%dDc" % n, 8, tx="CHARGER", extended=False)
    sig("Udc", 16, 16, factor=0.01052856, hi=690.09, unit="V")
    sig("Idc", 32, 16, factor=0.000839233, hi=55.0, unit="A")
    emit()

for n, ident in enumerate(ID_CHARGER_TMP, start=1):
    msg(ident, "Charger%dTemps" % n, 8, tx="CHARGER", extended=False)
    sig("Tmp1", 0, 8, offset=-40, lo=-40, hi=215, unit="degC")
    sig("Tmp2", 8, 8, offset=-40, lo=-40, hi=215, unit="degC")
    sig("TmpIn", 40, 8, offset=-40, lo=-40, hi=215, unit="degC")
    emit()

# ------------------------------------------- charger controller (our own stm32-teslacharger)

def inv_gain(gain, offset=0):
    """DBC (factor, offset) for an openinverter AddSend(gain, offset).

    CanMap transmits raw = physical * gain + offset, so decoding is
    physical = raw/gain - offset/gain. Rounded because the schema hash digests these as text and
    a repr that differs across Python versions would move the hash without the map changing.
    """
    return round(1.0 / gain, 10), round(-float(offset) / gain, 10)


msg(ID_CHGCTL_COMMAND, "ChargerCtlCommand", 8, tx="CHARGER", extended=False)
# The setpoint the controller is currently commanding. Observed 0x8214 -> 333.00 V.
sig("DcSetpoint", 0, 16, factor=0.01, hi=655.35, unit="V")
sig("CmdConstB2", 16, 8)                       # literal 0x14
_f, _o = inv_gain(32, 0xE)
sig("CmdOpmode", 24, 8, factor=_f, offset=_o, hi=7)
sig("CmdConstB6", 48, 8)                       # literal 0x90
sig("CmdConstB7", 56, 8)                       # literal 0x8C
emit()

for n, ident in enumerate(ID_CHGCTL_MODULE, start=1):
    msg(ident, "ChargerCtlModule%d" % n, 8, tx="CHARGER", extended=False)
    sig("ModConstB0", 0, 8)                    # literal 0x42
    # Literal 0xBB today. The source marks it "TODO: AC voltage", so if this ever stops being
    # constant in a log, the charger firmware gained a feature -- worth noticing.
    sig("ModAcVoltagePlaceholder", 8, 8)
    _f, _o = inv_gain(1500)
    sig("ModAcLimit", 16, 16, factor=_f, offset=_o, hi=43.69, unit="A")
    _f, _o = inv_gain(154, 100)
    sig("ModOpmode", 32, 8, factor=_f, offset=_o, hi=1)
    emit()

msg(ID_CHGCTL_IDENT, "ChargerCtlIdent", 8, tx="CHARGER", extended=False)
# Eight literals with no known meaning; byte 4 is not mapped at all and reads zero. Declared so
# a decode shows them and so a change would be visible.
for _b, _pos in enumerate((0, 8, 16, 24, 40, 48, 56)):
    sig("Ident_B%d" % (_pos // 8), _pos, 8)
emit()

msg(ID_CHGCTL_CHADEMO_LIMITS, "ChargerCtlChademoLimits", 8, tx="CHARGER", extended=False)
# Emitted as Param::version * 107, i.e. a constant produced by abusing the version number as a
# multiplier. Observed 428. If the firmware version ever changes, so does this "limit".
sig("ChademoMaxVoltage", 8, 16, hi=65535, unit="V")
sig("ChademoCurrentLimit", 24, 8, unit="A")
emit()

msg(ID_CHGCTL_CHADEMO_STATUS, "ChargerCtlChademoStatus", 8, tx="CHARGER", extended=False)
sig("ChademoUdc", 8, 16, unit="V")
sig("ChademoIdc", 24, 16, unit="A")
# 3 bits at gain 5, so opmode 1 sets bits 0 and 2 at once -- "charging" and "connector lock"
# in one write, per the source comment.
_f, _o = inv_gain(5)
sig("ChademoOpmode", 40, 3, factor=_f, offset=_o, hi=1)
emit()

# ------------------------------------------------- VCU -> charger commands (0x102, 0x103)

# Everything the charger receives here goes through CanMap::HandleRx -> Param::Set, which ignores
# any value outside the parameter's min/max and keeps the previous one. That is deliberate
# clamping -- a garbage frame cannot drive the charger out of range -- but it is silent: no fault,
# no log. A byte-swapped or zeroed field is indistinguishable from the charger never having been
# mapped at all, which is worth knowing when a decode of these frames looks correct but the
# charger is not following it.

msg(ID_CHGCMD_CHADEMO, "ChargerCmdChademo", 8, tx="VCU", extended=False)
# Upstream's CHAdeMO RX map, reused as the VCU's command frame. UdcLim is the termination point:
# the charger's CheckVoltage() trips its state machine to STOP once udc exceeds this for ten
# consecutive ticks, and STOP latches until AC is unplugged or enable drops.
sig("UdcLim", 8, 16, hi=420, unit="V")
sig("IdcSpnt", 24, 8, hi=45, unit="A")
# Only consulted while the charger's cancontrol parameter is set, which it is not on this car --
# enable comes from the hardware line. Left in the map because upstream put it there.
sig("CanEnable", 40, 1, hi=1)
sig("Soc", 48, 8, hi=100, unit="%")
emit()

msg(ID_CHGCMD_SETPOINT, "ChargerCmdSetpoint", 8, tx="VCU", extended=False)
# The CV regulation setpoint. The charger streams it straight out to the modules on 0x45C every
# 100 ms, so a change takes effect within one period and no state machine is involved.
sig("UdcSpnt", 0, 16, hi=420, unit="V")
# Which modules run, as a bitmask: 1|2|4. Latched into the hardware enable lines at the charger's
# WAITSTART -> ENABLE transition and not changeable afterwards. 0 is below the parameter minimum
# of 1, so sending 0 is the way to say "leave the module set alone".
sig("ChargerEna", 16, 8, hi=7)
# When set, the charger picks the module count itself from the advertised supply current and
# overwrites ChargerEna with the result.
sig("ChargerAuto", 24, 8, hi=1)
emit()

# ---------------------------------------------------------------- comments

comment_msg(ID_STATUS,
            "BMS status and faults, one per cell-sense scan (10 Hz, 2 Hz charging). "
            "Latched and instantaneous fault bits are both present on purpose: the dash lamp "
            "follows the instantaneous state while the inverter's 50 percent throttle limit "
            "follows the latched one, so a single-cycle sag half-throttles the car with only "
            "a lamp flicker. Logging both makes that divergence visible.")
comment_sig(ID_STATUS, "IoexpMask",
            "MCP_BMS_THREAD_MASK. Bits outside this mask are not owned by the BMS thread and "
            "the Ioexp_ signals say nothing about them.")
comment_sig(ID_STATUS, "Ioexp_G",
            "Mirror of the literal byte passed to MCP23017::write_mask(), so the log records "
            "what the dash was told rather than a reconstruction of it.")
comment_sig(ID_STATUS, "Gpi_Knob1Sw",
            "MCP23017 port B input bits, pin 8+n in bit n. RESERVED: the mainline firmware "
            "does not read the input side at all. Only the charge-control branch does, via "
            "ioexp->read_mask(MCP_BMS_THREAD_READ_MASK) for the CC/CV knob switches on pins 11 "
            "and 12. Held in the map so that branch can land without renumbering this frame. "
            "Transmit 0 until then, and note that 0 is indistinguishable from 'switches open' "
            "-- use the firmware version to tell 'not implemented' from 'nothing pressed'.")
comment_msg(ID_CELL_SUM,
            "CellAvg is packVoltage / 84, the same expression the live display uses at "
            "BmsThread.cpp:920. Note 84, cells in series, NOT 168 total cells -- the two "
            "strings are in parallel. The commented-out display block just above divides by "
            "168 and would read half the true value.")
comment_sig(ID_CELL_SUM, "CellMinIndex",
            "Flat cell index 0..167, identical to the indexing of Cell_000..Cell_167 and of "
            "the balancing mask. Display label is 'A'+index/28 and index%28+1; that geometry "
            "is deliberately not encoded on the wire because the pack layout has changed "
            "before.")
comment_msg(ID_DIE_BASE,
            "Die temperatures are whole degrees because the source is a uint8: "
            "raw*0.0001/0.0076 - 276. Encoding them at 0.1 degC would invent precision.")
comment_msg(ID_BAL_BASE,
            "One bit per cell, indexed identically to Cell_000..Cell_167. Commanded discharge "
            "state, not current flow -- discharge is muted around the measurement. "
            "popcount over all three frames must equal NumBalancing on the same scan.")
comment_msg(ID_SCHEMA,
            "Identifies the schema this stream was produced with, broadcast at 1 Hz so a "
            "consumer joining mid-stream attributes at most one second of data to an unknown "
            "schema. SchemaHash is a digest of the map's structure -- message ids, signal "
            "names, start bits, lengths, signedness and scaling -- and deliberately excludes "
            "comments, units and value tables, so rewording documentation does not invalidate "
            "every consumer's cache. Generated into a C header by generate_914_dbc.py so the "
            "broadcast value cannot drift from the DBC. Consumer rules: hash equal, proceed; "
            "hash differs but major equal, additive change, keep decoding known signals and "
            "log once; major differs, stop and quarantine, because decoding with the wrong "
            "major yields plausible wrong numbers rather than an error.")
comment_msg(ID_INTEGRITY,
            "Not part of SLCAN. Counter gaps expose lost frames and the CRC16 over every byte "
            "since the previous integrity frame exposes substitutions that the frame's own "
            "length check misses. Other tools can ignore this id.")
comment_sig(ID_INVERTER, "InvDcVoltage",
            "HV DC bus voltage. Scale is 0.1 V because the inverter's CAN map sends udc at "
            "gain 10 -- defined by that map entry, not inferred from a capture. Before "
            "2026-08-10 nothing was mapped here at all and these bytes read zero.",
            extended=False)
comment_msg(ID_VCU_CONTROL,
            "Transmitted by the VCU, so the proxy never receives it -- a CAN node does not see "
            "its own frames. It must be echoed into the log deliberately at transmit request, "
            "which is evidence of intent rather than proof of arbitration success. Only "
            "BmsLimit is verified; the rest of the layout is not yet mapped.", extended=False)
comment_msg(ID_INVERTER_SDO_REQ,
            "SDO read request from the VCU to the inverter, 0x600 + nodeid. The VCU polls "
            "pot (id 2015), pot2 (2016) and regenpreset (2051) round-robin, one outstanding at "
            "a time, at 10 Hz while opmode is not Off and 1 Hz otherwise. Parameters are named "
            "by unique ID -- index 0x2100 | (id >> 8), subIndex id & 0xFF -- and NOT by array "
            "position under index 0x2000, because positions shift between firmware builds. "
            "Transmitted by the VCU, so like 0x03F it is echoed into the log at transmit "
            "request rather than received. A request with no matching reply before the next "
            "poll is a dropped sample; it is visible in the log as a 0x601 with no 0x581 after "
            "it, which is why no separate loss counter exists. Strictly a diagnostic path: SDO "
            "has no sequence counter, no CRC and no timeout semantics, so nothing in the "
            "control loop may depend on it.", extended=False)
comment_msg(ID_INVERTER_SDO_RESP,
            "SDO reply from the inverter, 0x580 + nodeid. The requested index and subIndex are "
            "echoed, so a request and its reply pair exactly even if the reply is late. "
            "SdoCmd 0x43 is a successful read; 0x80 is an abort, and then SdoData is the abort "
            "code rather than a value -- 0x06020000 means the index is wrong for this firmware "
            "build, which retrying cannot fix. For a parameter read SdoData is s32fp: divide by "
            "32, signed.", extended=False)
comment_sig(ID_CHARGER_AC[0], "Iac",
            "9 bits at bit 41, spanning byte 5 bits 1-7 and byte 6 bits 0-1. Main.cpp used to "
            "read 15 bits here (data[5]>>1 plus data[6]<<7), taking six bits above the field "
            "that the charger maps to nothing; fixed 2026-08-22 to mask byte 6 to two bits.",
            extended=False)
comment_sig(ID_CHARGER_AC[0], "HwAcLim",
            "Hardware AC current limit reported by the module. All three modules send their own "
            "and the charger maps every one onto a single hwaclim parameter, so on the charger "
            "side the last frame received wins.", extended=False)
comment_msg(ID_CHARGER_DC[0],
            "Module DC output, measured at the module rather than at the pack. The charger sums "
            "Idc across the three for its idc, and takes the highest Udc of the three as its "
            "udc -- which is the value CheckVoltage() compares against udclim to terminate a "
            "charge.", extended=False)
comment_msg(ID_CHARGER_TMP[0],
            "Module temperatures, offset -40 degC. Tmp1 and Tmp2 are module-internal, TmpIn is "
            "the inlet. Which of the three track real conditions has not been checked against a "
            "logged session yet.", extended=False)
comment_msg(ID_CHGCMD_SETPOINT,
            "VCU to charger. UdcSpnt is the CV regulation target; the termination point in the "
            "other charge mode is UdcLim on 0x102 instead. Which of the two carries the "
            "knob-selected target is what the dash mode switch chooses.", extended=False)

emit()
# CHFLAGS from stm32-teslacharger/include/param_prj.h. A bitfield: bit 0 Enabled, bit 1
# Fault, bit 2 CheckAlive. Only two bits are mapped, so CheckAlive cannot appear.
for _ac_id in ID_CHARGER_AC:
    emit('VAL_ %d Flag 0 "None" 1 "Enabled" 2 "Fault" 3 "EnabledFault" ;' % _ac_id)
emit('VAL_ %d DiagCode 0 "None" 1 "PecFailure" 2 "BmsFault" 3 "ThreadStartFailure" '
     '4 "InvalidMessage" 5 "SdoAbort" ;' % (ID_DIAG | EXT))
# openinverter's own OPMODES and DIRS, verbatim from stm32-sine/param_prj.h.
emit('VAL_ %d InvOpmode 0 "Off" 1 "Run" 2 "ManualRun" 3 "Boost" 4 "Buck" 5 "Sine" '
     '6 "AcHeat" ;' % ID_INVERTER_ST)
# CanSdo's command bytes, composed from the CANopen bitfields in libopeninv/src/cansdo.cpp:
# SDO_READ is 2<<5, SDO_READ_REPLY adds EXPEDITED (1<<1) and SIZE_SPECIFIED (1), SDO_WRITE is
# 1<<5 with the same two, SDO_WRITE_REPLY is 3<<5. Only 0x40, 0x43 and 0x80 are produced by the
# VCU's poll; the write commands are listed because the host-TX path can emit them.
for _sdo_id in (ID_INVERTER_SDO_REQ, ID_INVERTER_SDO_RESP):
    emit('VAL_ %d SdoCmd 64 "Read" 67 "ReadReply" 35 "Write" 96 "WriteReply" 128 "Abort" ;'
         % _sdo_id)
# The tri-state din_ocur / din_desat encoding, openinverter's own.
for _tri in ("InvIo_Ocur", "InvIo_Desat"):
    emit('VAL_ %d %s 0 "Error" 1 "Ok" 2 "na" ;' % (ID_INVERTER_ST, _tri))
emit()

# ---------------------------------------------------------------- schema identity

# Sorted so that reordering the generator's emission without changing any definition leaves the
# hash alone. Only a real structural change moves it.
canonical = "\n".join(sorted(_schema))
SCHEMA_HASH = int.from_bytes(hashlib.sha256(canonical.encode("ascii")).digest()[:4], "big")

# Stamp the identity into the DBC itself, as a file-level comment.
#
# Without this a consumer holds the map but no record of what it should hash to, so it can receive
# the SchemaId broadcast and still have nothing to compare it against -- the check that the
# broadcast exists to enable is unimplementable from the DBC alone. schema_id.h covers the
# firmware half of that comparison; this covers the decoder half.
#
# Safe by construction: the digest above runs over _schema, which carries only what changes how a
# byte decodes. Comments are excluded from it deliberately, so a comment carrying the hash cannot
# perturb the hash it carries. Adding this therefore does not move the schema, and firmware
# already flashed stays valid -- no reflash.
#
# SCHEMA_HASH=0x... is a fixed token rather than prose, so a consumer matches it exactly instead
# of parsing an English sentence that a later reword would silently break.
emit('CM_ "914 EV telemetry schema %d.%d, SCHEMA_HASH=0x%08X. '
     'Generated by generate_914_dbc.py -- do not hand-edit.";'
     % (SCHEMA_MAJOR, SCHEMA_MINOR, SCHEMA_HASH))
emit()

header_path = None
for _a in sys.argv[1:]:
    if _a.startswith("--header="):
        header_path = _a.split("=", 1)[1]

if header_path:
    with open(header_path, "w", encoding="ascii", newline="\n") as fh:
        fh.write("""// GENERATED by tools/generate_914_dbc.py -- do not edit.
//
// Broadcast in SchemaId (0x%08X) so a consumer can tell whether the stream it is decoding
// matches the DBC it has loaded. Regenerate whenever the map changes; the hash is derived from
// the map's structure, so it cannot be forgotten the way a hand-maintained version can.
#ifndef SCHEMA_ID_H
#define SCHEMA_ID_H

#define SCHEMA_MAJOR 0x%02Xu
#define SCHEMA_MINOR 0x%02Xu
#define SCHEMA_HASH  0x%08Xu

#endif // SCHEMA_ID_H
""" % (ID_SCHEMA, SCHEMA_MAJOR, SCHEMA_MINOR, SCHEMA_HASH))
    sys.stderr.write("schema %d.%d hash 0x%08X -> %s\n"
                     % (SCHEMA_MAJOR, SCHEMA_MINOR, SCHEMA_HASH, header_path))
else:
    sys.stderr.write("schema %d.%d hash 0x%08X (pass --header=PATH to emit the C header)\n"
                     % (SCHEMA_MAJOR, SCHEMA_MINOR, SCHEMA_HASH))

print("\n".join(out))
