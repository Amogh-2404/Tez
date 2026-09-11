#!/usr/bin/env python3
"""Regenerate the repository's SVG artwork. Run from any directory."""
import argparse
from html import escape
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--assets-only', action='store_true', help='Regenerate banner and social artwork without rewriting architecture diagrams.')
args = parser.parse_args()

ROOT = Path(__file__).resolve().parents[2]
INK = "#1b211e"
MUTED = "#52615a"
PAPER = "#f7f6f1"
LINE = "#cbd1c9"
ACCENT = "#c47f19"


def text(x, y, value, size=18, fill=INK, weight=400):
    return (f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" '
            f'font-weight="{weight}">{escape(value)}</text>')


def base(title, description, height=520):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="{height}" '
            f'viewBox="0 0 1100 {height}" role="img" aria-labelledby="title desc">',
            f'<title id="title">{escape(title)}</title><desc id="desc">{escape(description)}</desc>',
            '<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" '
            'markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M 0 0 L 10 5 L 0 10 z" fill="{MUTED}"/></marker></defs>',
            f'<rect width="1100" height="{height}" fill="{PAPER}"/>',
            '<g font-family="Helvetica,Arial,sans-serif">',
            text(40, 44, "TEZ / ARCHITECTURE", 13, MUTED, 700),
            text(40, 86, title, 30, INK, 700),
            f'<path d="M40 108 H1060" stroke="{LINE}"/>']


def box(parts, x, y, width, title, lines=(), height=100, accent=False):
    parts.append(f'<rect x="{x}" y="{y}" width="{width}" height="{height}" '
                 f'fill="#ffffff" stroke="{ACCENT if accent else LINE}" stroke-width="1.5"/>')
    parts.append(text(x+18, y+32, title, 20, INK, 700))
    for i, line in enumerate(lines):
        parts.append(text(x+18, y+59+i*24, line, 16, MUTED))


def arrow(parts, x1, y1, x2, y2, bend=None):
    path = f'M{x1} {y1} '
    path += f'H{bend} V{y2} H{x2}' if bend is not None else f'L{x2} {y2}'
    parts.append(f'<path d="{path}" fill="none" stroke="{MUTED}" stroke-width="1.5" marker-end="url(#arrow)"/>')


def save(parts, filename, note, height=520):
    if args.assets_only:
        return
    parts += [text(40, height-26, note, 15, MUTED), '</g></svg>']
    (ROOT / 'diagrams' / filename).write_text('\n'.join(parts)+'\n')


p = base("A small server with explicit boundaries", "Async sessions route to immutable configuration or a confined file reader and validated file cache.", 540)
box(p, 40, 155, 175, "Client", ("HTTP/1.x", "TCP connection"), 120)
box(p, 260, 155, 260, "Listener + session", ("Asio accept / read / write", "Beast parser + serializer"), 120, True)
box(p, 610, 145, 440, "Built-ins + configured routes", ("Method dispatch; immutable startup snapshot",), 92)
box(p, 610, 275, 440, "Static file reader", ("Descriptor-relative opens; size checks",), 92)
box(p, 610, 405, 440, "File cache", ("Metadata checked before every hit",), 78)
arrow(p, 215, 215, 260, 215)
arrow(p, 520, 197, 610, 191, 565)
arrow(p, 520, 244, 610, 321, 565)
arrow(p, 830, 367, 830, 405)
p.append(text(260, 328, "One retained input buffer", 19, INK, 700))
p.append(text(260, 357, "One ordered response stream", 19, INK, 700))
p.append(text(260, 397, "Deadlines + admission limit", 17, MUTED))
save(p, '01-system-overview.svg', "Socket I/O is asynchronous. Route work, file reads, and logging execute synchronously in handlers.", 540)

p = base("One request, one ordered response", "Read headers, validate, optionally continue, read body, dispatch, write and reuse or close.", 590)
box(p, 40, 150, 290, "01  Read headers", ("Persistent connection buffer", "Header deadline + size limit"), 114)
box(p, 405, 150, 290, "02  Validate", ("Host, target, framing, limits", "100 Continue when expected"), 114)
box(p, 770, 150, 290, "03  Read body", ("Fixed length or chunked", "Body deadline + payload limit"), 114)
arrow(p, 330, 207, 405, 207); arrow(p, 695, 207, 770, 207)
box(p, 770, 355, 290, "04  Dispatch", ("Built-in / JSON route / file", "No next-request dispatch yet"), 114)
box(p, 405, 355, 290, "05  Write", ("Response lifetime retained", "HEAD sends headers only"), 114)
box(p, 40, 355, 290, "06  Reuse or close", ("Keep-alive + request cap", "Keep unread bytes for next read"), 114)
arrow(p, 915, 264, 915, 355); arrow(p, 770, 412, 695, 412); arrow(p, 405, 412, 330, 412)
p.append(f'<path d="M40 412 H20 V207 H40" fill="none" stroke="{ACCENT}" stroke-width="1.5" marker-end="url(#arrow)"/>')
p.append(text(403, 315, "Parsing errors close the connection.", 17, MUTED))
save(p, '02-request-flow.svg', "TCP read boundaries are not HTTP message boundaries. Unused input survives the request.", 590)

p = base("Workers run handlers; sessions own state", "Multiple I/O threads execute one io_context; each session's strand serializes callbacks.", 555)
for i, x in enumerate([40, 395, 750]):
    box(p, x, 145, 310, f"I/O worker {i+1}", ("io_context::run()",), 85)
p.append(f'<path d="M195 230 V269 H905 V230 M550 230 V269 M550 269 V308" fill="none" stroke="{MUTED}" stroke-width="1.5"/>')
box(p, 40, 308, 310, "Session A / strand", ("Read → handle → write", "Serialized for this connection"), 116, True)
box(p, 395, 308, 310, "Session B / strand", ("Read → handle → write", "May run beside another session"), 116, True)
box(p, 750, 308, 310, "Session C / strand", ("Read → handle → write", "No fixed worker assignment"), 116, True)
p.append(f'<path d="M195 269 V308 M905 269 V308" stroke="{MUTED}" stroke-width="1.5"/>')
p.append(text(40, 478, "File reads, JSON work, cache locks, and stderr writes can occupy a worker.", 18, INK, 700))
save(p, '03-threading-model.svg', "A strand orders handlers. It does not create a thread or make synchronous operations nonblocking.", 555)

p = base("A cache hit must still identify the file", "File access validation precedes cache lookup; metadata and TTL determine freshness; entries and bytes are bounded.", 555)
box(p, 40, 150, 330, "Open + validate file", ("Root confinement and regular file", "Identity, size, timestamps"), 118)
box(p, 420, 150, 640, "Match cached identity + metadata + age", ("Valid hit: return body and promote recency", "Miss: bounded read; metadata recheck before insertion"), 118, True)
arrow(p, 370, 209, 420, 209)
p.append(text(40, 325, "Most recently used", 16, MUTED, 700))
p.append(text(867, 325, "Least recently used", 16, MUTED, 700))
for i, label in enumerate(['File A', 'File B', 'File C', 'File D']):
    box(p, 40+i*260, 347, 240, label, ("body + metadata",), 87)
    if i < 3: arrow(p, 280+i*260, 390, 300+i*260, 390)
p.append(text(40, 484, "50 entries  /  32 MiB logical bytes  /  60-second TTL", 21, INK, 700))
save(p, '04-lru-cache.svg', "Evict from the tail until both budgets fit. The logical-byte budget is not total process memory.", 555)

p = base("Static access begins at an open directory", "Decode once, reject unsafe components, open relative to root with symlink rejection, inspect file and validate cache.", 665)
items = [
("01", "Decode the request path once", "Remove query; reject malformed escapes and encoded separators."),
("02", "Validate every path component", "Reject traversal, dotfiles, empty segments, and control bytes."),
("03", "Open relative to the root descriptor", "Walk directories with openat; reject symbolic links at each step."),
("04", "Inspect the opened file", "Regular files only; enforce the 16 MiB size limit before reading."),
("05", "Revalidate cache identity and metadata", "A cache hit follows file access checks; changed files miss."),
]
for i, (n, title, desc) in enumerate(items):
    y = 140+i*93
    p.append(text(40, y+29, n, 21, ACCENT, 700))
    p.append(text(100, y+27, title, 21, INK, 700))
    p.append(text(100, y+55, desc, 17, MUTED))
    if i < 4: p.append(f'<path d="M100 {y+72} H1060" stroke="{LINE}"/>')
save(p, '05-security-defense.svg', "The static tree must remain trusted. These checks are not isolation from hostile filesystem administrators.", 665)

p = base("TCP lifecycle meets the session lifecycle", "An Asio acceptor accepts sockets; Beast sessions read and write; completion, deadlines and signals end ownership.", 600)
rows = [
("Listen", "tcp::acceptor", "Bind configured address and port; accept asynchronously."),
("Accept", "Session admission", "Count active sessions; close excess accepted sockets."),
("Read", "Beast parser + buffer", "Retain unread bytes; enforce header and body limits."),
("Respond", "Async write + strand", "Keep response alive; finish write before next dispatch."),
("Close", "Session teardown", "Peer close, I/O error, deadline, request cap, or shutdown."),
]
for i, (phase, obj, desc) in enumerate(rows):
    y=144+i*78
    p.append(text(40,y+25,phase,20,ACCENT,700))
    p.append(text(200,y+25,obj,20,INK,700))
    p.append(text(480,y+25,desc,16,MUTED))
    p.append(f'<path d="M40 {y+47} H1060" stroke="{LINE}"/>')
save(p, '06-tcp-lifecycle.svg', "Shutdown cancels network work. It does not promise a grace period for draining in-flight responses.", 600)


def hero(height, social=False):
    title_y = 225 if social else 175
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="{height}" '
             f'viewBox="0 0 1280 {height}" role="img" aria-labelledby="title desc">',
             '<title id="title">Tez — a compact C++17 HTTP server.</title>',
             '<desc id="desc">JSON routes and static files for local development.</desc>',
             f'<rect width="1280" height="{height}" fill="#151b17"/>',
             '<g font-family="Helvetica,Arial,sans-serif">',
             text(64, 58, "R. AMOGH / OPEN SOURCE", 14, '#b4bcb3', 700),
             text(60, title_y, "Tez", 124, '#f7f6f1', 700),
             text(64, title_y+68, "A compact C++17 HTTP server.", 38, '#f7f6f1', 400),
             text(64, title_y+118, "JSON routes and static files for local development.", 24, '#b4bcb3'),
             f'<path d="M64 {height-75} H1216" stroke="#3a453c"/>',
             text(64, height-38, "BOOST.BEAST  /  BOOST.ASIO  /  MIT", 14, '#b4bcb3', 700),
             text(1095, height-38, "tez.ramogh.com", 14, '#b4bcb3', 700)]
    cy = height//2-35
    for i in range(3):
        y=cy-85+i*78
        parts.append(f'<path d="M925 {y} H1120 L1170 {y+34} H1060" fill="none" stroke="{["#e9b457", "#718473", "#465d4b"][i]}" stroke-width="12"/>')
        parts.append(f'<circle cx="915" cy="{y}" r="7" fill="#e9b457"/>')
    parts.append('</g></svg>')
    return '\n'.join(parts)+'\n'

(ROOT/'docs/assets/tez-banner.svg').write_text(hero(430))
(ROOT/'docs/assets/tez-social.svg').write_text(hero(640, True))
