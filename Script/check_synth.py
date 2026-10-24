import importlib.util
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SPEC = importlib.util.spec_from_file_location(
    "cross", ROOT / "Script" / "cross_check_xlrd.py")
CROSS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CROSS)

FREE = 0xFFFFFFFF
ENDOF = 0xFFFFFFFE
FATSCT = 0xFFFFFFFD


def rec(rid: int, body: bytes) -> bytes:
    return struct.pack("<HH", rid, len(body)) + body


def utf16z(text: str) -> bytes:
    return text.encode("utf-16-le") + b"\x00\x00"


def dir_entry(name: bytes, etype: int, child: int, start: int,
              size: int) -> bytes:
    ent = bytearray(128)
    ent[:len(name)] = name
    struct.pack_into("<H", ent, 64, len(name))
    ent[66] = etype
    ent[67] = 1
    struct.pack_into("<I", ent, 68, FREE)
    struct.pack_into("<I", ent, 72, FREE)
    struct.pack_into("<I", ent, 76, child)
    struct.pack_into("<I", ent, 116, start)
    struct.pack_into("<I", ent, 120, size)
    return bytes(ent)


def cfb(stream: bytes) -> bytes:
    sector = 512
    n = (len(stream) + sector - 1) // sector
    fat = [FATSCT, ENDOF]
    for i in range(n):
        fat.append(i + 3 if i + 1 < n else ENDOF)
    fat += [FREE] * (sector // 4 - len(fat))
    dir_sector = (dir_entry(utf16z("Root Entry"), 5, 1, ENDOF, 0)
                  + dir_entry(utf16z("Workbook"), 2, FREE, 2, len(stream)))
    dir_sector += b"\x00" * (sector - len(dir_sector))
    head = bytearray(512)
    head[:8] = bytes([0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1])
    head[28] = 0xFE
    head[29] = 0xFF
    struct.pack_into("<H", head, 30, 9)
    struct.pack_into("<H", head, 32, 6)
    struct.pack_into("<I", head, 44, 1)
    struct.pack_into("<I", head, 48, 1)
    struct.pack_into("<I", head, 60, ENDOF)
    struct.pack_into("<I", head, 68, ENDOF)
    struct.pack_into("<I", head, 72, 0)
    for i in range(109):
        struct.pack_into("<I", head, 76 + i * 4, 0 if i == 0 else FREE)
    body = stream + b"\x00" * (n * sector - len(stream))
    return bytes(head) + struct.pack("<128I", *fat) + dir_sector + body


def dims_oversize() -> bytes:
    pos = 8 + 13 + 4 + (4200 + 4)
    stream = b"".join([
        rec(0x0809, struct.pack("<HH", 0x0600, 0x0005)),
        rec(0x0085, struct.pack("<IBBBB", pos, 0, 0, 1, 0) + b"S"),
        rec(0x000A, b""),
        rec(0x0001, b"\x00" * 4200),
        rec(0x0809, struct.pack("<HH", 0x0600, 0x0010)),
        rec(0x0200, struct.pack("<IIHHH", 0, 5, 0, 3, 0)),
        rec(0x0203, struct.pack("<HHHd", 0, 0, 0, 1.5)),
        rec(0x000A, b""),
    ])
    return cfb(stream)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_synth.py <xls_csv_dump.exe>")
        return 2
    CROSS.DUMP = Path(sys.argv[1])
    out = ROOT / "Build" / "synth"
    out.mkdir(parents=True, exist_ok=True)
    path = out / "dims-oversize.xls"
    path.write_bytes(dims_oversize())
    problems = CROSS.cells_agree(path)
    if problems:
        for line in problems[:10]:
            print("FAIL", line)
        return 1
    print("PASS", path.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
