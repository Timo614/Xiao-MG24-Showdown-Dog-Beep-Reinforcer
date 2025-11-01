from __future__ import annotations
import os, re
from typing import List, Tuple, Dict, Optional

# -------------------- CONFIG --------------------
FILES = [
    "/content/Dogsmensch.sub", 
    "/content/Dogsmaz.sub",
]
OUT_DIR: Optional[str] = None         
CHUNK_WIDTH = 24                      
INVERT = False                        
USE_PROGMEM = False                   
# ------------------------------------------------

def _sanitize_identifier(name: str) -> str:
    ident = re.sub(r'\W', '_', name)
    if re.match(r'^\d', ident): ident = '_' + ident
    return ident

def _guard_from_name(name: str) -> str:
    return f"{_sanitize_identifier(name).upper()}_H"

def _format_int_list(values: List[int], per_line: int) -> str:
    out = []
    for i in range(0, len(values), per_line):
        chunk = values[i:i+per_line]
        trail = "," if i + per_line < len(values) else ""
        out.append("    " + ", ".join(str(x) for x in chunk) + trail)
    return "\n".join(out)

def _extract_meta(content: str) -> Tuple[Optional[int], Optional[str]]:
    freq = None; proto = None
    m = re.search(r'^\s*Frequency\s*:\s*([0-9]+)\s*$', content, re.MULTILINE)
    if m:
        try: freq = int(m.group(1))
        except ValueError: pass
    m = re.search(r'^\s*Protocol\s*:\s*([^\r\n]+?)\s*$', content, re.MULTILINE)
    if m: proto = m.group(1).strip()
    return freq, proto

def parse_sub_file(path: str) -> Tuple[List[int], Optional[int], Optional[str]]:
    """Return (timings, frequency_hz, protocol)."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    freq_hz, proto = _extract_meta(content)

    raw_lines: List[str] = []
    for line in content.splitlines():
        s = line.lstrip()
        if s.startswith("RAW_Data:"):
            raw_lines.append(s[len("RAW_Data:"):].strip())

    if not raw_lines:
        raw_lines = [m.group(1) for m in re.finditer(r'RAW_Data:\s*([^\r\n]+)', content)]

    timings: List[int] = []
    if raw_lines:
        for tok in " ".join(raw_lines).split():
            try:
                timings.append(int(tok))
            except ValueError:
                tok2 = tok.rstrip(",;")
                try: timings.append(int(tok2))
                except ValueError: pass

    return timings, freq_hz, proto

def generate_header_text(
    array_name: str,
    timings: List[int],
    chunk_size: int,
    frequency_hz: Optional[int],
    protocol: Optional[str],
    use_progmem: bool,
) -> str:
    name_id = _sanitize_identifier(array_name)
    guard = _guard_from_name(array_name)

    meta = ["// Generated from Flipper Zero SubGhz RAW capture"]
    if frequency_hz:
        meta.append(f"// Frequency: {frequency_hz} Hz (~{frequency_hz/1_000_000:.3f} MHz)")
    if protocol:
        meta.append(f"// Protocol: {protocol}")
    meta.append(f"// Total timing values: {len(timings)}")
    meta_comment = "\n".join(meta)

    body = _format_int_list(timings, chunk_size)

    if use_progmem:
        array_decl = f"static const int32_t {name_id}[] PROGMEM = {{\n{body}\n}};"
        prog_hdr = "#if defined(__AVR__)\n#include <avr/pgmspace.h>\n#endif\n"
    else:
        array_decl = f"static const int32_t {name_id}[] = {{\n{body}\n}};"
        prog_hdr = ""

    return f"""#ifndef {guard}
#define {guard}

#include <stdint.h>
{prog_hdr}{meta_comment}

{array_decl}

static constexpr size_t {name_id}Count = sizeof({name_id}) / sizeof({name_id}[0]);

#endif // {guard}
"""

def ensure_out_dir(base_path: str, out_dir: Optional[str]) -> str:
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        return out_dir
    d = os.path.dirname(base_path) or "."
    try:
        os.makedirs(d, exist_ok=True)
        test_path = os.path.join(d, ".write_test")
        with open(test_path, "w") as _t: _t.write("x")
        os.remove(test_path)
        return d
    except Exception:
        os.makedirs("/mnt/data", exist_ok=True)
        return "/mnt/data"

def make_headers_for_files(paths: List[str]) -> Dict[str, Dict[str, object]]:
    results: Dict[str, Dict[str, object]] = {}
    used_names: set[str] = set()

    for p in paths:
        if not os.path.exists(p):
            continue

        timings, freq_hz, proto = parse_sub_file(p)
        if not timings:
            continue

        if INVERT:
            timings = [-t for t in timings]

        stem = os.path.splitext(os.path.basename(p))[0]
        array_name = _sanitize_identifier(stem)

        base = array_name
        i = 2
        while array_name in used_names:
            array_name = f"{base}_{i}"; i += 1
        used_names.add(array_name)

        header_text = generate_header_text(
            array_name=array_name,
            timings=timings,
            chunk_size=CHUNK_WIDTH,
            frequency_hz=freq_hz,
            protocol=proto,
            use_progmem=USE_PROGMEM,
        )

        out_dir = ensure_out_dir(p, OUT_DIR)
        header_path = os.path.join(out_dir, f"{array_name}.h")
        with open(header_path, "w", encoding="utf-8") as f:
            f.write(header_text)

        results[p] = {
            "array_name": array_name,
            "header_path": header_path,
            "count": len(timings),
            "freq_hz": freq_hz,
            "protocol": proto,
            "timings": timings,  
        }

    return results

results = make_headers_for_files(FILES)

if not results:
    print("No headers generated. Check your FILES list/paths.")
else:
    print("Generated headers:")
    for src, info in results.items():
        print(f"- {src}")
        print(f"  -> array: {info['array_name']}")
        print(f"  -> count: {info['count']}")
        print(f"  -> freq : {info['freq_hz']}")
        print(f"  -> proto: {info['protocol']}")
        print(f"  -> file : {info['header_path']}")
        print()

