import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def decode_float(token: str) -> float:
    token = token.strip()
    value = int(token, 0)
    return struct.unpack("<f", struct.pack("<I", value & 0xFFFFFFFF))[0]


def cpp_float(token: str) -> str:
    value = decode_float(token)
    text = format(value, ".9g")
    if "e" not in text and "." not in text:
        text += ".0"
    return text + "f"


def parse_function(filename: str):
    source = (ROOT / "analysis_pa" / "functions" / filename).read_text(encoding="utf-8")
    events = []
    for match in re.finditer(r"FUN_0042faa0\([^,]+,([^,]+),([^,]+),([^\)]+)\);", source):
        events.append((match.start(), "vector", match.groups()))
    for match in re.finditer(r"FUN_0040c2c0\(param_1,(.*?)\);", source, re.S):
        events.append((match.start(), "structure", match.group(1)))
    events.sort(key=lambda event: event[0])

    vectors = []
    placements = []
    for _, kind, payload in events:
        if kind == "vector":
            vectors.append(payload)
            continue
        if len(vectors) < 2:
            raise RuntimeError(f"Missing vectors before structure call in {filename}")
        arguments = [part.strip() for part in payload.replace("\r", " ").replace("\n", " ").split(",")]
        model = int(arguments[1], 0)
        rotation, position = vectors[-2], vectors[-1]
        placements.append((model, position, rotation))
    return placements


def render_array(name: str, placements) -> str:
    lines = [f"static constexpr SceneryPlacement {name}[] = {{"]
    for model, position, rotation in placements:
        p = ", ".join(cpp_float(value) for value in position)
        r = ", ".join(cpp_float(value) for value in rotation)
        lines.append(f"    {{{model}, {{{p}}}, {{{r}}}}},")
    lines.append("};")
    return "\n".join(lines)


battle = parse_function("0x416c80.c")
campaign = parse_function("0x412a50.c")
if len(battle) != 132 or len(campaign) != 171:
    raise RuntimeError(f"Unexpected placement counts: battle={len(battle)} campaign={len(campaign)}")

output = "// Generated mechanically from the two original pa.exe scenery constructors.\n"
output += render_array("battleScenery", battle) + "\n\n"
output += render_array("campaignScenery", campaign) + "\n"
(Path(__file__).resolve().parent / "scenery_placements.inc").write_text(output, encoding="utf-8", newline="\n")
print(f"battle={len(battle)} campaign={len(campaign)}")
