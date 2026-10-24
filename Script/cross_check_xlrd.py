import subprocess
import sys
from pathlib import Path

try:
    import xlrd
except ImportError:
    print("SKIP: xlrd not installed")
    raise SystemExit(77)

ROOT = Path(__file__).resolve().parents[1]
DUMP = ROOT / "Build" / "xls_csv_dump.exe"


def dump_lines(path: Path) -> list[str]:
    res = subprocess.run([str(DUMP), str(path)], capture_output=True,
                         text=True, encoding="utf-8")
    if res.returncode != 0:
        raise RuntimeError(res.stdout + res.stderr)
    return res.stdout.splitlines()


def sheet_lines(lines: list[str], name: str) -> list[str]:
    header = f"=== {name}"
    if header not in lines:
        raise RuntimeError(f"sheet header missing: {header}")
    rest = lines[lines.index(header) + 1:]
    out = []
    for line in rest:
        if line.startswith("=== "):
            break
        out.append(line)
    return out


def cells_agree(path: Path) -> list[str]:
    problems = []
    book = xlrd.open_workbook(str(path))
    lines = dump_lines(path)
    if not lines[0].startswith("sheets:"):
        return [f"{path.name}: bad header {lines[0]!r}"]
    for si in range(book.nsheets):
        sh = book.sheet_by_index(si)
        rows = sheet_lines(lines, sh.name)
        for r in range(sh.nrows):
            if r >= len(rows):
                problems.append(f"{path.name}:{sh.name} row {r} missing in dump")
                break
            parts = rows[r].split(";")
            for c in range(sh.ncols):
                v = sh.cell_value(r, c)
                t = sh.cell_type(r, c)
                m = parts[c].strip(chr(34)) if c < len(parts) else "<missing>"
                if t == xlrd.XL_CELL_ERROR:
                    agree = m == "#ERR"
                elif t == xlrd.XL_CELL_BOOLEAN:
                    agree = m in ("true", "false")
                elif isinstance(v, float):
                    try:
                        mv = float(m)
                        agree = abs(mv - v) <= max(1e-9, abs(v) * 1e-9)
                    except ValueError:
                        agree = v == 0.0 and m in ("", "<missing>")
                else:
                    agree = m == str(v)
                if not agree:
                    problems.append(
                        f"{path.name}:{sh.name} r{r}c{c} dump={m!r} xlrd={v!r}")
    return problems


def main() -> int:
    global DUMP
    args = []
    for arg in sys.argv[1:]:
        if arg.startswith("--dump="):
            DUMP = Path(arg[len("--dump="):])
        else:
            args.append(arg)
    paths = [Path(a) for a in args]
    if not paths:
        paths = sorted((ROOT / "Test" / "files").glob("*.xls"))
    if not paths:
        print("no .xls files given and Test/files empty")
        return 2
    bad = 0
    for p in paths:
        try:
            problems = cells_agree(p)
        except Exception as exc:
            problems = [f"{p}: {exc}"]
        if problems:
            bad += 1
            for line in problems[:10]:
                print("FAIL", line)
        else:
            print("PASS", p.name)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
