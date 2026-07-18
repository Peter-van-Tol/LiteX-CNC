from pathlib import Path
import csv
from typing import TypedDict

class CSRRegister(TypedDict):
    address: int
    size: int
    type: str

class CsrMap:
    def __init__(self, csv_path: str | Path):
        self.path = Path(csv_path)
        self.registers: dict[str, CSRRegister] = {}
        self.constants: dict[str, str | int | None] = {}
        self._load()

    def _load(self) -> None:
        with self.path.open(newline="") as csv_file:
            reader = csv.reader(csv_file)
            for row in reader:
                if not row or row[0].startswith("#"):
                    continue
                kind = row[0]
                if kind == "csr_register":
                    name = row[1]
                    self.registers[name] = {
                        "address": int(row[2], 0),
                        "size": int(row[3], 0),
                        "type": row[4],
                    }
                elif kind == "constant":
                    value = row[2]
                    if value in {"None", "none", "null", "NULL"}:
                        parsed: str | int | None = None
                    else:
                        try:
                            parsed = int(value, 0)
                        except ValueError:
                            parsed = value
                    self.constants[row[1]] = parsed

    def resolve(self, value: str | int) -> tuple[int, int, str]:
        if isinstance(value, int):
            return value, 1, f"0x{value:08X}"
        if value in self.registers:
            register = self.registers[value]
            return int(register["address"]), int(register["size"]), value
        return int(value, 0), 1, value