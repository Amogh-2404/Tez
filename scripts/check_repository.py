#!/usr/bin/env python3
"""Validate local documentation links and checked-in JSON/SVG assets."""
import json
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    names = subprocess.check_output(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'],
        cwd=ROOT,
    ).decode().split('\0')
    failures = []
    checked = 0
    for name in sorted(set(names)):
        path = ROOT / name
        if not name or not path.is_file():
            continue
        if path.suffix in ('.cpp', '.hpp', '.py', '.sh', '.yml', '.yaml', '.json', '.svg') or path.name in ('CMakeLists.txt', 'Dockerfile'):
            for line_number, line in enumerate(path.read_text().splitlines(), 1):
                if line.rstrip() != line:
                    failures.append(f'{name}:{line_number}: trailing whitespace')
        if path.suffix in ('.json', '.svg'):
            try:
                if path.suffix == '.json':
                    json.loads(path.read_text())
                else:
                    ET.parse(path)
            except (ValueError, ET.ParseError) as error:
                failures.append(f'{name}: {error}')
        if path.suffix != '.md':
            continue
        content = re.sub(r'```.*?```', '', path.read_text(), flags=re.S)
        targets = re.findall(r'\]\(([^\s)]+)(?:\s+"[^"]*")?\)', content)
        targets += re.findall(r'<img\b[^>]*\bsrc=[\'"]([^\'"]+)', content)
        for target in targets:
            target = target.strip('<>')
            parts = urlsplit(target)
            if parts.scheme or parts.netloc or not parts.path:
                continue
            resolved = (path.parent / unquote(parts.path)).resolve()
            if (resolved != ROOT and ROOT not in resolved.parents) or not resolved.exists():
                failures.append(f'{name}: missing local target {target}')
            checked += 1
    for failure in failures:
        print(failure, file=sys.stderr)
    if failures:
        return 1
    print(f'Checked {checked} local documentation links and JSON/SVG assets.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
