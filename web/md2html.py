#!/usr/bin/env python3
"""Minimal Markdown -> HTML converter for the Xinu manual.

Handles the constructs used in docs/MANUAL-KERNEL.md: fenced code, headings,
tables, lists, blockquotes, rules, inline code/bold/italic/links.
"""
import html
import re
import sys


def slug(text):
    text = text.replace("`", "").replace("*", "")
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    text = text.lower()
    text = re.sub(r"[^\w\s\-áéíóúüñç]", "", text, flags=re.UNICODE)
    text = re.sub(r"\s+", "-", text.strip())
    text = re.sub(r"-+", "-", text)
    return text


def inline(text):
    # Protect inline code first.
    codes = []

    def keep(m):
        codes.append(m.group(1))
        return "\x00%d\x00" % (len(codes) - 1)

    text = re.sub(r"`([^`]+)`", keep, text)
    text = html.escape(text, quote=False)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", text)
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', text)
    for i, c in enumerate(codes):
        text = text.replace("\x00%d\x00" % i, "<code>%s</code>" % html.escape(c))
    return text


def split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    cells, cur, tick = [], "", False
    for ch in line:
        if ch == "`":
            tick = not tick
            cur += ch
        elif ch == "|" and not tick:
            cells.append(cur.strip())
            cur = ""
        else:
            cur += ch
    cells.append(cur.strip())
    return cells


def convert(md):
    lines = md.splitlines()
    # Drop the hand-written TOC: from "## Índice" to the next standalone "---".
    out_lines = []
    i = 0
    while i < len(lines):
        if lines[i].strip() == "## Índice":
            i += 1
            while i < len(lines) and lines[i].strip() != "---":
                i += 1
            if i < len(lines):
                i += 1
            continue
        out_lines.append(lines[i])
        i += 1
    lines = out_lines

    body, toc = [], []
    i = 0
    while i < len(lines):
        line = lines[i]
        s = line.strip()

        if s.startswith("```"):
            lang = s[3:].strip()
            i += 1
            code = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code.append(lines[i])
                i += 1
            i += 1
            cls = ' class="lang-%s"' % lang if lang else ""
            body.append("<pre><code%s>%s</code></pre>" % (cls, html.escape("\n".join(code))))
            continue

        m = re.match(r"^(#{1,6})\s+(.*)$", line)
        if m:
            level = len(m.group(1))
            text = m.group(2).strip()
            sid = slug(text)
            body.append('<h%d id="%s">%s</h%d>' % (level, sid, inline(text), level))
            if level in (2, 3):
                toc.append((level, sid, re.sub(r"[`*]", "", text)))
            i += 1
            continue

        if re.match(r"^---+\s*$", line):
            body.append("<hr>")
            i += 1
            continue

        if s.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append(lines[i])
                i += 1
            if len(rows) >= 2 and re.match(r"^\|?[\s:|-]+\|?$", rows[1].strip()):
                head = split_row(rows[0])
                data = rows[2:]
            else:
                head = None
                data = rows
            body.append("<table>")
            if head:
                body.append("<thead><tr>" + "".join("<th>%s</th>" % inline(c) for c in head) + "</tr></thead>")
            body.append("<tbody>")
            for r in data:
                body.append("<tr>" + "".join("<td>%s</td>" % inline(c) for c in split_row(r)) + "</tr>")
            body.append("</tbody></table>")
            continue

        if s.startswith(">"):
            quote = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                quote.append(lines[i].strip()[1:].strip())
                i += 1
            body.append("<blockquote>%s</blockquote>" % inline(" ".join(quote)))
            continue

        m = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", line)
        if m:
            ordered = m.group(2)[0].isdigit()
            tag = "ol" if ordered else "ul"
            items = []
            while i < len(lines):
                mm = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)$", lines[i])
                if not mm:
                    break
                items.append(mm.group(3))
                i += 1
            body.append("<%s>%s</%s>" % (tag, "".join("<li>%s</li>" % inline(x) for x in items), tag))
            continue

        if not s:
            i += 1
            continue

        para = []
        while i < len(lines):
            t = lines[i].strip()
            if not t or t.startswith("#") or t.startswith("```") or t.startswith("|") \
               or t.startswith(">") or re.match(r"^(\s*)([-*]|\d+\.)\s+", lines[i]) \
               or re.match(r"^---+\s*$", lines[i]):
                break
            para.append(t)
            i += 1
        body.append("<p>%s</p>" % inline(" ".join(para)))

    # Insert generated TOC right before the first h2.
    toc_html = ['<nav class="toc"><h2>Índice</h2><ul>']
    for level, sid, text in toc:
        if level == 2:
            toc_html.append('<li class="l2"><a href="#%s">%s</a></li>' % (sid, html.escape(text)))
        else:
            toc_html.append('<li class="l3"><a href="#%s">%s</a></li>' % (sid, html.escape(text)))
    toc_html.append("</ul></nav>")

    html_out = []
    inserted = False
    for block in body:
        if not inserted and block.startswith("<h2"):
            html_out.append("".join(toc_html))
            inserted = True
        html_out.append(block)
    if not inserted:
        html_out.insert(0, "".join(toc_html))
    return "\n".join(html_out)


PAGE = """<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Manual del kernel Xinu · ESP32-S3</title>
<style>
:root{--bg:#0a0f1d;--card:#131d33;--line:#26344f;--txt:#e8eef7;--muted:#a9b8cf;
--dim:#7c8ca6;--acc:#6ea8fe;--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);
font:16px/1.65 system-ui,-apple-system,"Segoe UI",Roboto,Ubuntu,sans-serif}
main{max-width:58rem;margin:0 auto;padding:2rem 1.2rem 5rem}
a{color:var(--acc);text-decoration:none}a:hover{text-decoration:underline}
h1{font-size:2rem;line-height:1.2;margin:.2rem 0 .4rem;letter-spacing:-.02em}
h2{font-size:1.35rem;margin:2.4rem 0 .6rem;padding-top:.6rem;border-top:1px solid var(--line)}
h3{font-size:1.08rem;margin:1.7rem 0 .4rem;color:#dbe7fa}
h4{font-size:.98rem;margin:1.2rem 0 .3rem}
p{margin:.65rem 0;color:#d5e0f0}
code{font-family:var(--mono);background:#0b1322;border:1px solid #1f2c45;border-radius:4px;
padding:.08em .35em;font-size:.88em}
pre{background:#080d18;border:1px solid var(--line);border-radius:10px;padding:1rem;overflow:auto;
font-size:.84rem;line-height:1.55}
pre code{background:none;border:0;padding:0;font-size:inherit}
table{width:100%;border-collapse:collapse;margin:1rem 0;font-size:.9rem}
th,td{text-align:left;padding:.5rem .6rem;border-bottom:1px solid #1e2b44;vertical-align:top}
th{color:var(--dim);font-size:.78rem;text-transform:uppercase;letter-spacing:.06em}
blockquote{margin:1rem 0;padding:.7rem 1rem;border-left:3px solid var(--acc);background:#0f1a2e;
border-radius:0 8px 8px 0;color:var(--muted)}
hr{border:0;border-top:1px solid var(--line);margin:2rem 0}
ul,ol{padding-left:1.4rem}li{margin:.25rem 0;color:#d5e0f0}
.back{display:inline-block;margin-bottom:1rem;font-size:.9rem;color:var(--muted);
border:1px solid var(--line);border-radius:8px;padding:.4rem .7rem}
.back:hover{color:var(--txt);border-color:var(--acc)}
.toc{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:1rem 1.2rem;margin:1.6rem 0}
.toc h2{border:0;margin:.1rem 0 .5rem;padding:0;font-size:1rem}
.toc ul{list-style:none;padding:0;margin:0;columns:2;column-gap:2rem}
.toc li{margin:.15rem 0;font-size:.9rem}
.toc .l3{padding-left:1rem;color:var(--dim)}
@media(max-width:720px){.toc ul{columns:1}}
</style>
</head>
<body>
<main>
<a class="back" href="index.html">← Volver al dashboard</a>
@@BODY@@
</main>
</body>
</html>
"""


def main():
    src = sys.argv[1]
    dst = sys.argv[2]
    with open(src, encoding="utf-8") as f:
        md = f.read()
    out = PAGE.replace("@@BODY@@", convert(md))
    with open(dst, "w", encoding="utf-8") as f:
        f.write(out)
    print("wrote %s (%d bytes)" % (dst, len(out.encode("utf-8"))))


if __name__ == "__main__":
    main()
