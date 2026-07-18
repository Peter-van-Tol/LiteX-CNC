import re
from pathlib import Path

class AliasMap:
    PIN_PATTERN = re.compile(r"^(.+)\.gpio\.(\d+)\.out$")

    def __init__(self, alias_path: str | Path):
        self.path = Path(alias_path)
        self.index_to_aliases: dict[int, list[str]] = {}
        self.alias_to_index: dict[str, int] = {}
        self.short_alias_to_indices: dict[str, list[int]] = {}
        self._load()

    @classmethod
    def from_csr_path(cls, csr_path: str | Path) -> "AliasMap | None":
        alias_path = Path(csr_path).with_name("alias.hal")
        if not alias_path.exists():
            return None
        return cls(alias_path)

    def _load(self) -> None:
        with self.path.open() as alias_file:
            for raw_line in alias_file:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) != 4 or parts[0] != "alias" or parts[1] != "pin":
                    continue
                match = self.PIN_PATTERN.match(parts[2])
                if not match:
                    continue

                index = int(match.group(2))

                alias_name = parts[3].rsplit(".", 1)[0]
                if index not in self.index_to_aliases:
                    self.index_to_aliases[index] = []
                self.index_to_aliases[index].append(alias_name)
                self.alias_to_index[alias_name.lower()] = index

                short_alias = alias_name.rsplit(".", 1)[-1].lower()
                if short_alias not in self.short_alias_to_indices:
                    self.short_alias_to_indices[short_alias] = []
                self.short_alias_to_indices[short_alias].append(index)

    def resolve(self, token: str | int) -> tuple[int, str]:
        if isinstance(token, int):
            return token, f"{token}"
        token_lower = token.lower()
        if token_lower in self.alias_to_index:
            return self.alias_to_index[token_lower], token
        if token_lower in self.short_alias_to_indices:
            indices = self.short_alias_to_indices[token_lower]
            if len(indices) == 1:
                return indices[0], token
            raise Exception(f"alias {token!r} is ambiguous; use a full alias name or a numeric index")
        return int(token, 0), token

    def labels_for(self, index: int) -> list[str]:
        return self.index_to_aliases.get(index, [])

    def iter_pins(self):
        for index in sorted(self.index_to_aliases):
            yield index, self.index_to_aliases[index]