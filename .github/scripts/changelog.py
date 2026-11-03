#!/usr/bin/env python3
# changelog.py: CHANGELOG.md is the one source of the public changelog page, and the release pipeline publishes it
# ==========================================================================================
# [Claude 2026-10-30] Joe: "Ideally the changelog gets auto-pushed and it's not Claude or Joe updating each time."
# Standard library only (runs on GitHub's Ubuntu runner and on Windows).
#
#   changelog.py check <version>              CHANGELOG.md has an approved entry for <version>: a dated
#                                             "## x.y.z (YYYY-MM-DD)" heading, at least one line, no draft marker
#   changelog.py render <version> [--prev-version V --prev-mac URL --prev-win URL]
#                                             prints the HTML block for every entry up to <version>, newest first
#   changelog.py publish <version>            on the live site (tag only, in the publish job):
#                                             1. checks the entry (as above)
#                                             2. finds the previous version's installers in the media library
#                                             3. renders the block and writes it into the changelog page's block,
#                                                changing nothing else on the page
#                                             4. sets the version on the home page's version line, if it has one
#                                             5. clears Elementor's cache, reads both pages back and checks them live
#   changelog.py publish <version> --dry-run --page-json F [--home-json F] [--media-json F] --out DIR
#                                             the same against local copies of the page data; writes the result
#                                             to DIR and touches no site
#
# The changelog block on the page (the only thing the pipeline writes there):
#   - Elementor: the one Text Editor widget whose CSS Classes (Advanced tab) include "wla-changelog".
#     Its whole text is replaced. Everything else on the page (title, intro, other widgets) is Joe's.
#   - Plain WordPress page (not built with Elementor): the text between <!-- wla-changelog:start --> and
#     <!-- wla-changelog:end --> in the page content.
# The home page's version line: the Text Editor widget whose CSS Classes include "wla-version" (or the class given
# with --version-class: on a site that sells several plugins each has its own, e.g. wla-version-<slug>), for example
# "1.0.2, released 28 January 2026". Its one x.y.z number becomes the new version and a date in it (either
# "28 January 2026" or "January 28, 2026") the release date, in the same style; any other words or links stay.
#
# CHANGELOG.md format (newest first):
#   ## 1.1.0 (2026-10-30)        or "## 1.1.0 (release date)": the pipeline then uses the v1.1.0 tag's date in
#                                America/New_York, the site's time zone (an annotated tag's own date; a lightweight
#                                tag has none, so the day the release runs). A date written in the heading wins.
#   <!-- draft: ... -->          optional; while it is there the pipeline refuses to publish this version
#   **Mac**                      optional group label
#   - one change per bullet      **bold** and [text](https://...) work; HTML is escaped
#
# Environment for publish: WORDPRESS_SITE, WORDPRESS_USERNAME, WORDPRESS_APPLICATION_PASSWORD (the existing secrets).
# Tests: .github/scripts/test_changelog.py (no network).
# ==========================================================================================
import argparse
import datetime
import html
import http.client
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

BLOCK_CLASS = "wla-changelog"
VERSION_CLASS = "wla-version"
START = "<!-- wla-changelog:start -->"
END = "<!-- wla-changelog:end -->"
NOTE = ("<!-- Written by the release pipeline from CHANGELOG.md in the plugin's repo. "
        "Edit CHANGELOG.md, not this block: it is replaced on every release. -->")
PLACEHOLDER = "release date"   # "## 1.1.0 (release date)": the pipeline uses the release tag's date
HEADING_RE = re.compile(r"^## (\d+\.\d+\.\d+) \((\d{4}-\d{2}-\d{2}|" + PLACEHOLDER + r")\)\s*$")
MONTHS = "January February March April May June July August September October November December".split()
DATE_TEXT_RE = re.compile(r"\b\d{1,2} (?:" + "|".join(MONTHS) + r") \d{4}\b|\b(?:" + "|".join(MONTHS) + r") \d{1,2}, \d{4}\b")
DRAFT_RE = re.compile(r"<!--\s*draft\b", re.IGNORECASE)
VERSION_RE = re.compile(r"\b\d+\.\d+\.\d+\b")


class Fail(Exception):
    pass


class Unknown(Fail):
    """The site could not be read back, so what it shows now is not known."""


for _stream in (sys.stdout, sys.stderr):   # a Windows console is not UTF-8; never fail on a printed character
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass


# ------------------------------------------------------------------------------------------
# CHANGELOG.md
# ------------------------------------------------------------------------------------------
def vkey(v):
    return tuple(int(x) for x in v.split("."))


def parse(text):
    """Returns the entries newest first: {version, date, draft, items: [("group"|"bullet"|"text", str)]}."""
    entries, cur = [], None
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if line.startswith("## "):
            m = HEADING_RE.match(line)
            if not m:
                raise Fail(f"CHANGELOG.md line {n}: '{line}' is not '## x.y.z (YYYY-MM-DD)' or '## x.y.z ({PLACEHOLDER})'.")
            date = None if m.group(2) == PLACEHOLDER else m.group(2)
            if date:
                try:
                    datetime.date.fromisoformat(date)
                except ValueError:
                    raise Fail(f"CHANGELOG.md line {n}: {date} is not a real date.")
            cur = {"version": m.group(1), "date": date, "draft": False, "items": []}
            entries.append(cur)
            continue
        if cur is None or not line.strip():
            continue
        s = line.strip()
        if DRAFT_RE.match(s):
            cur["draft"] = True
        elif s.startswith("<!--"):
            continue
        elif re.match(r"^[-*] ", s):
            cur["items"].append(["bullet", s[2:].strip()])
        elif re.match(r"^\*\*[^*]+\*\*:?$", s):
            cur["items"].append(["group", s.strip("*:").strip()])
        elif raw.startswith("  ") and cur["items"] and cur["items"][-1][0] == "bullet":
            cur["items"][-1][1] += " " + s        # a bullet continued on the next line
        elif s.startswith("#"):
            raise Fail(f"CHANGELOG.md line {n}: only '## x.y.z (YYYY-MM-DD)' headings are allowed inside the entries.")
        else:
            cur["items"].append(["text", s])
    seen = set()
    for e in entries:
        if e["version"] in seen:
            raise Fail(f"CHANGELOG.md has two entries for {e['version']}.")
        seen.add(e["version"])
    order = [vkey(e["version"]) for e in entries]
    if order != sorted(order, reverse=True):
        raise Fail("CHANGELOG.md entries are not newest first.")
    return entries


def check(entries, version):
    e = next((e for e in entries if e["version"] == version), None)
    if e is None:
        raise Fail(f"CHANGELOG.md has no entry for {version}. Add '## {version} (YYYY-MM-DD)' and its lines.")
    if e["draft"]:
        raise Fail(f"The {version} entry in CHANGELOG.md is still marked as a draft. "
                   "Joe approves it by deleting the '<!-- draft' line; nothing is published until then.")
    if not any(k == "bullet" for k, _ in e["items"]):
        raise Fail(f"The {version} entry in CHANGELOG.md has no bullet points.")
    if entries[0]["version"] != version:
        raise Fail(f"CHANGELOG.md's newest entry is {entries[0]['version']}, not {version}.")
    return e


def previous_version(entries, version):
    older = [e["version"] for e in entries if vkey(e["version"]) < vkey(version)]
    return older[0] if older else None


# ------------------------------------------------------------------------------------------
# Release dates: the date written in the entry wins; "(release date)" takes the v<version> tag's date, as a day in
# the site's time zone (America/New_York)
# ------------------------------------------------------------------------------------------
SITE_TZ = "America/New_York"


def site_day(when):
    """The calendar day of an aware datetime in America/New_York."""
    try:
        from zoneinfo import ZoneInfo
        return when.astimezone(ZoneInfo(SITE_TZ)).date()
    except Exception:   # no time zone data (Windows without tzdata): the US Eastern rule, daylight time from the
        utc = when.astimezone(datetime.timezone.utc)   # 2nd Sunday of March 2 AM to the 1st Sunday of November 2 AM
        y = utc.year
        mar = datetime.datetime(y, 3, 8 + (6 - datetime.date(y, 3, 8).weekday()) % 7, 7, tzinfo=datetime.timezone.utc)
        nov = datetime.datetime(y, 11, 1 + (6 - datetime.date(y, 11, 1).weekday()) % 7, 6, tzinfo=datetime.timezone.utc)
        return (utc - datetime.timedelta(hours=4 if mar <= utc < nov else 5)).date()


def tag_date(version, current=False, git_dir="."):
    """The release date of v<version>, as a day in New York: an annotated tag's own date; for a lightweight tag (no
    date of its own) the day the release runs if it is the version being released, else its commit's date.
    None if there is no such tag."""
    import subprocess
    try:
        out = subprocess.run(["git", "-C", git_dir, "for-each-ref", f"refs/tags/v{version}",
                              "--format=%(objecttype) %(taggerdate:iso-strict) %(creatordate:iso-strict)"],
                             capture_output=True, text=True, check=True).stdout.split()
    except (OSError, subprocess.CalledProcessError):
        return None
    if not out:
        return None
    if out[0] == "tag" and len(out) == 3:
        when = datetime.datetime.fromisoformat(out[1])
    elif current:
        when = datetime.datetime.now(datetime.timezone.utc)
    else:
        when = datetime.datetime.fromisoformat(out[-1])
    return site_day(when).isoformat()


def resolve_dates(entries, version, release_date=None, lookup=tag_date):
    """Fills in each "(release date)" entry up to <version>. release_date (YYYY-MM-DD) is for <version> only."""
    for e in entries:
        if e["date"] or vkey(e["version"]) > vkey(version):
            continue
        d = release_date if (e["version"] == version and release_date) else lookup(e["version"], e["version"] == version)
        if not d:
            raise Fail(f"CHANGELOG.md's {e['version']} entry says '({PLACEHOLDER})' and there is no v{e['version']} tag "
                       f"to take the date from. Write the date in its heading: '## {e['version']} (YYYY-MM-DD)'.")
        e["date"] = d
    return entries


# ------------------------------------------------------------------------------------------
# HTML
# ------------------------------------------------------------------------------------------
def inline(s):
    out, pos = [], 0
    for m in re.finditer(r"\*\*(.+?)\*\*|\[([^\]]+)\]\((https?://[^)\s]+|/[^)\s]*|mailto:[^)\s]+)\)|`([^`]+)`", s):
        out.append(html.escape(s[pos:m.start()], quote=False))
        if m.group(1) is not None:
            out.append("<strong>" + html.escape(m.group(1), quote=False) + "</strong>")
        elif m.group(2) is not None:
            out.append('<a href="' + html.escape(m.group(3)) + '">' + html.escape(m.group(2), quote=False) + "</a>")
        else:
            out.append(html.escape(m.group(4), quote=False))
        pos = m.end()
    out.append(html.escape(s[pos:], quote=False))
    return "".join(out)


def nice_date(iso):
    d = datetime.date.fromisoformat(iso)
    return f"{d.day} {MONTHS[d.month - 1]} {d.year}"   # 28 January 2026, as on the site's other pages


def render(entries, version, prev=None):
    """The block's HTML: every entry up to <version>, newest first. prev = (version, mac_url, win_url) or None."""
    shown = [e for e in entries if vkey(e["version"]) <= vkey(version)]
    if not shown or shown[0]["version"] != version:
        raise Fail(f"CHANGELOG.md has no entry for {version}.")
    out = [START, NOTE]
    for i, e in enumerate(shown):
        out.append(f'<h4 class="wla-changelog-version">{e["version"]}, {nice_date(e["date"])}</h4>')
        in_list = False
        for kind, txt in e["items"]:
            if kind == "bullet":
                if not in_list:
                    out.append("<ul>")
                    in_list = True
                out.append("<li>" + inline(txt) + "</li>")
                continue
            if in_list:
                out.append("</ul>")
                in_list = False
            if kind == "group":
                out.append('<p class="wla-changelog-group"><strong>' + inline(txt) + "</strong></p>")
            else:
                out.append("<p>" + inline(txt) + "</p>")
        if in_list:
            out.append("</ul>")
        if i == 0 and prev:
            pv, mac, win = prev
            out.append('<p class="wla-changelog-previous">Need the previous version? ' + html.escape(pv) +
                       ' for <a href="' + html.escape(mac) + '">Mac</a> &middot; <a href="' + html.escape(win) +
                       '">Windows</a></p>')
    out.append(END)
    return "\n".join(out)


# ------------------------------------------------------------------------------------------
# Page data: find the block, replace it, prove nothing else changed
# ------------------------------------------------------------------------------------------
def classes(el):
    return (el.get("settings") or {}).get("_css_classes", "").split()


def find_widgets(elements, css_class, path=()):
    found = []
    for i, el in enumerate(elements or []):
        p = path + (i,)
        if el.get("elType") == "widget" and css_class in classes(el):
            found.append((p, el))
        found += find_widgets(el.get("elements"), css_class, p + ("elements",))
    return found


def diff_paths(a, b, path=()):
    if isinstance(a, dict) and isinstance(b, dict):
        out = []
        for k in sorted(set(a) | set(b)):
            out += diff_paths(a.get(k), b.get(k), path + (k,)) if (k in a and k in b) else [path + (k,)]
        return out
    if isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        out = []
        for i, (x, y) in enumerate(zip(a, b)):
            out += diff_paths(x, y, path + (i,))
        return out
    return [] if a == b else [path]


def current_block(page):
    """The block's HTML as it is on the page now ("" if none)."""
    meta = page.get("meta") or {}
    raw = meta.get("_elementor_data") or ""
    if meta.get("_elementor_edit_mode") == "builder" and raw:
        hits = find_widgets(json.loads(raw), BLOCK_CLASS)
        return hits[0][1]["settings"].get("editor", "") if len(hits) == 1 else ""
    content = (page.get("content") or {}).get("raw", "")
    if START in content and END in content:
        return content[content.index(START):content.index(END) + len(END)]
    return ""


def dates_on_page(block_html):
    """{version: YYYY-MM-DD} from the version headings the pipeline wrote last time."""
    out = {}
    for v, d in re.findall(r'class="wla-changelog-version">(\d+\.\d+\.\d+), ([^<]+)</h4>', block_html):
        m = re.fullmatch(r"(\d{1,2}) (" + "|".join(MONTHS) + r") (\d{4})", d.strip())
        if m:
            out[v] = datetime.date(int(m.group(3)), MONTHS.index(m.group(2)) + 1, int(m.group(1))).isoformat()
    return out


def replace_block(page, block_html):
    """page: the REST page (context=edit). Returns ("elementor", new _elementor_data string) or ("content", new raw)."""
    meta = page.get("meta") or {}
    raw = meta.get("_elementor_data") or ""
    if meta.get("_elementor_edit_mode") == "builder" and raw:
        data = json.loads(raw)
        hits = [h for h in find_widgets(data, BLOCK_CLASS)]
        if len(hits) != 1:
            raise Fail(f"The changelog page needs exactly one widget with the CSS class '{BLOCK_CLASS}' "
                       f"(Advanced > CSS Classes); it has {len(hits)}. Nothing was changed.")
        path, el = hits[0]
        if el.get("widgetType") != "text-editor":
            raise Fail(f"The '{BLOCK_CLASS}' widget is a {el.get('widgetType')}, not a Text Editor. Nothing was changed.")
        new = json.loads(raw)
        target = new
        for k in path:
            target = target[k]
        target["settings"]["editor"] = block_html
        diffs = diff_paths(data, new)
        allowed = path + ("settings", "editor")
        if any(d != allowed for d in diffs):
            raise Fail(f"Internal check: more than the changelog block would change ({diffs}). Nothing was changed.")
        return "elementor", json.dumps(new, ensure_ascii=False, separators=(",", ":"))
    content = (page.get("content") or {}).get("raw", "")
    if content.count(START) != 1 or content.count(END) != 1 or content.index(START) > content.index(END):
        raise Fail(f"The changelog page is not built with Elementor and its content has no single "
                   f"{START} ... {END} pair. Nothing was changed.")
    a, b = content.index(START), content.index(END) + len(END)
    return "content", content[:a] + block_html + content[b:]


def set_version_line(page, version, date=None, css_class=None):
    """Returns the home page's new _elementor_data, or None when it has no version line. A date written in the line
    ("28 January 2026" or "January 28, 2026") becomes <date> in the same style; a line with no date keeps none."""
    meta = page.get("meta") or {}
    raw = meta.get("_elementor_data") or ""
    if not raw:
        return None
    css_class = css_class or VERSION_CLASS
    data = json.loads(raw)
    hits = find_widgets(data, css_class)
    if not hits:
        return None
    if len(hits) != 1 or hits[0][1].get("widgetType") != "text-editor":
        raise Fail(f"The home page should have exactly one Text Editor widget with the CSS class '{css_class}'; it has "
                   f"{[el.get('widgetType') for _, el in hits]}. Nothing was changed.")
    new = json.loads(raw)
    allowed = []
    for path, el in hits:
        target = new
        for k in path:
            target = target[k]
        field = "editor"
        txt = target["settings"].get(field, "")
        if len(VERSION_RE.findall(txt)) != 1:
            raise Fail(f"The home page's '{css_class}' widget {el.get('id')} should hold exactly one x.y.z "
                       f"version; it reads '{txt}'. Nothing was changed.")
        txt = VERSION_RE.sub(version, txt)
        if date:
            d = datetime.date.fromisoformat(date)
            txt = DATE_TEXT_RE.sub(lambda m: (f"{MONTHS[d.month - 1]} {d.day}, {d.year}" if "," in m.group(0)
                                              else f"{d.day} {MONTHS[d.month - 1]} {d.year}"), txt)
        target["settings"][field] = txt
        allowed.append(path + ("settings", field))
    if any(d not in allowed for d in diff_paths(data, new)):
        raise Fail("Internal check: more than the version line would change. Nothing was changed.")
    return json.dumps(new, ensure_ascii=False, separators=(",", ":"))


def version_line_texts(page, css_class=None):
    raw = (page.get("meta") or {}).get("_elementor_data") or ""
    if not raw:
        return []
    return [(el["settings"].get("editor") or el["settings"].get("title") or "")
            for _, el in find_widgets(json.loads(raw), css_class or VERSION_CLASS)]


# ------------------------------------------------------------------------------------------
# The site
# ------------------------------------------------------------------------------------------
class LiveSite:
    def __init__(self):
        try:
            self.site = os.environ["WORDPRESS_SITE"].rstrip("/")
            user, pw = os.environ["WORDPRESS_USERNAME"], os.environ["WORDPRESS_APPLICATION_PASSWORD"]
        except KeyError as k:
            raise Fail(f"{k.args[0]} is not set.")
        import base64
        self.auth = "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode()

    def req(self, method, path, body=None, auth=True, raw_url=None):
        url = raw_url or self.site + path
        headers = {"User-Agent": "WLA-release-pipeline", "Accept": "application/json"}
        if auth:
            headers["Authorization"] = self.auth
        data = None
        if body is not None:
            data = json.dumps(body).encode()
            headers["Content-Type"] = "application/json"
        for attempt in range(3):
            try:
                with urllib.request.urlopen(urllib.request.Request(url, data=data, method=method, headers=headers),
                                            timeout=90) as r:
                    return r.status, r.read().decode("utf-8", "replace")
            except urllib.error.HTTPError as e:
                if e.code >= 500 and method == "GET" and attempt < 2:
                    time.sleep(5)
                    continue
                try:
                    return e.code, e.read().decode("utf-8", "replace")
                except (OSError, http.client.HTTPException) as e2:
                    raise Fail(f"{method} {url} answered HTTP {e.code} and the answer was cut off: {e2}")
            except (OSError, http.client.HTTPException) as e:   # URLError, resets, timeouts, truncated answers
                if method == "GET" and attempt < 2:
                    time.sleep(5)
                    continue
                raise Fail(f"{method} {url} failed: {e}")

    def json(self, method, path, body=None):
        code, txt = self.req(method, path, body)
        try:
            out = json.loads(txt)
        except ValueError:
            out = None
        if code // 100 != 2 or out is None:
            raise Fail(f"{method} {path} answered HTTP {code}: {txt[:400]}")
        return out

    def page(self, pid):
        return self.json("GET", f"/wp-json/wp/v2/pages/{pid}?context=edit")

    def front_page_id(self):
        return int(self.json("GET", "/wp-json/wp/v2/settings").get("page_on_front") or 0)

    def media_url(self, title):
        q = urllib.parse.quote(title)
        items = self.json("GET", f"/wp-json/wp/v2/media?search={q}&per_page=100&_fields=id,title,source_url,date")
        hits = sorted((m for m in items if html.unescape(m["title"]["rendered"]) == title),
                      key=lambda m: m["date"], reverse=True)
        return hits[0]["source_url"] if hits else None

    def write_page(self, pid, kind, value):
        body = {"meta": {"_elementor_data": value}} if kind == "elementor" else {"content": value}
        self.json("POST", f"/wp-json/wp/v2/pages/{pid}", body)

    def clear_cache(self):
        code, txt = self.req("DELETE", "/wp-json/elementor/v1/cache")
        if code // 100 != 2:
            raise Fail(f"Clearing Elementor's cache answered HTTP {code}: {txt[:300]}")

    def public(self, url, bust=True):
        """What a visitor gets. bust=True adds a unique query string, so no page cache can answer; bust=False asks
        for the ordinary address, as a visitor does (the site's page cache may answer that)."""
        if not bust:
            return self.req("GET", None, auth=False, raw_url=url)
        sep = "&" if "?" in url else "?"
        return self.req("GET", None, auth=False, raw_url=f"{url}{sep}nc={int(time.time() * 1000)}")

    def reachable(self, url):
        code, _ = self.req("HEAD", None, auth=False, raw_url=url)
        return code // 100 == 2


class FileSite:
    """Dry run: local copies of the pages (REST JSON, context=edit) and of the media list; never any network."""

    def __init__(self, page_json, home_json, media_json, out):
        self.pages = {}
        for f in filter(None, [page_json, home_json]):
            p = json.load(open(f, encoding="utf-8"))
            p = p.get("data", p)
            self.pages[int(p["id"])] = p
        self.home = int(json.load(open(home_json, encoding="utf-8")).get("id")) if home_json else 0
        self.media = json.load(open(media_json, encoding="utf-8")) if media_json else []
        self.out = out
        os.makedirs(out, exist_ok=True)

    def page(self, pid):
        if pid not in self.pages:
            raise Fail(f"Dry run: no local copy of page {pid}.")
        return self.pages[pid]

    def front_page_id(self):
        return self.home

    def media_url(self, title):
        hits = [m for m in self.media if html.unescape(m["title"]["rendered"]) == title]
        return hits[0]["source_url"] if hits else None

    def write_page(self, pid, kind, value):
        name = f"page-{pid}.{'elementor_data.json' if kind == 'elementor' else 'content.html'}"
        open(os.path.join(self.out, name), "w", encoding="utf-8", newline="\n").write(value)
        p = self.pages[pid]
        if kind == "elementor":
            p["meta"]["_elementor_data"] = value
        else:
            p["content"]["raw"] = value

    def clear_cache(self):
        pass


def visible_text(fragment):
    """What a reader sees in an HTML fragment, normalised for comparing (WordPress turns ' into a curly quote, wraps
    lines in <p>, and so on)."""
    t = re.sub(r"<!--.*?-->", " ", fragment, flags=re.S)
    t = html.unescape(re.sub(r"<[^>]+>", " ", t))
    for a, b in (("\u2018", "'"), ("\u2019", "'"), ("\u201c", '"'), ("\u201d", '"'), ("\u2013", "-"),
                 ("\u2014", "-"), ("\u2026", "..."), ("\u00a0", " "), ("\u2032", "'")):
        t = t.replace(a, b)
    return re.sub(r"\s+", " ", t).strip()


def links(fragment):
    return sorted(html.unescape(u) for u in re.findall(r'href="([^"]+)"', fragment))


def served_widget(body, css_class):
    """The inside of the one rendered widget with this CSS class in a page's HTML (up to its own closing </div>),
    or None."""
    scan = re.sub(r"<!--.*?-->", lambda c: " " * len(c.group(0)), body, flags=re.S)   # comments out, same offsets
    m = list(re.finditer(r'<div\b[^>]*\bclass="elementor-element\b[^"]*\s' + re.escape(css_class) + r'[\s"][^>]*>', scan))
    if len(m) != 1:
        return None
    depth, start = 1, m[0].end()
    for t in re.finditer(r"<div\b|</div\s*>", scan[start:]):
        depth += 1 if t.group(0).startswith("<div") else -1
        if depth == 0:
            return body[start:start + t.start()]
    return None


def served_block(body):
    """The changelog block as served: between the markers (kept in the HTML on both Elementor and plain pages)."""
    if body.count(START) != 1 or body.count(END) != 1:
        return None
    return body[body.index(START):body.index(END) + len(END)]


def installer_title(plugin, version, platform):
    # Must match the titles the "Upload Installers to WordPress" step gives the uploads (test_changelog.py checks it)
    return f"{plugin} Installer v{version} for {platform}"


def publish(site, entries, version, page_id, plugin, dry_run=False, release_date=None, lookup=tag_date,
            version_class=VERSION_CLASS):
    check(entries, version)
    drafts = [e["version"] for e in entries if e["draft"] and vkey(e["version"]) <= vkey(version)]
    if drafts:
        raise Fail(f"CHANGELOG.md entries still marked as drafts would be published: {', '.join(drafts)}. "
                   "Delete their '<!-- draft' lines first. Nothing was changed.")
    log = print
    page = site.page(page_id)
    if not dry_run and page.get("status") != "publish":
        raise Fail(f"The changelog page {page_id} is '{page.get('status')}', not published. Nothing was changed.")
    # An entry headed "(release date)" keeps the date it was first published with: an older one on a later release,
    # and this one on a re-run (a lightweight tag has no date of its own, so asking again could give another day)
    published = dates_on_page(current_block(page))
    resolve_dates(entries, version, release_date,
                  lambda v, current=False: published.get(v) or lookup(v, current))
    log(f"{version} is dated {nice_date(entries[0]['date'])}")

    prev = None
    pv = previous_version(entries, version)
    if pv:
        mac, win = site.media_url(installer_title(plugin, pv, "Mac")), site.media_url(installer_title(plugin, pv, "Windows"))
        if mac and win:
            if not dry_run:
                for u in (mac, win):
                    if not site.reachable(u):
                        raise Fail(f"The previous version's installer {u} does not download. Nothing was changed.")
            prev = (pv, mac, win)
            log(f"Previous version {pv}: {mac} and {win}")
        else:
            log(f"::warning::No installers for {pv} in the media library (Mac: {mac}, Windows: {win}); "
                "the page shows no 'Need the previous version?' line.")

    block = render(entries, version, prev)
    kind, new_value = replace_block(page, block)

    home_id = site.front_page_id()
    home_new = None
    if home_id and home_id != page_id:
        home = site.page(home_id)
        home_new = set_version_line(home, version, entries[0]["date"], version_class)
        if home_new is None:
            log(f"The home page ({home_id}) has no '{version_class}' widget; no version line to update.")

    if dry_run:
        site.write_page(page_id, kind, new_value)
        if home_new is not None:
            site.write_page(home_id, "elementor", home_new)
        open(os.path.join(site.out, "block.html"), "w", encoding="utf-8", newline="\n").write(block)
        log(f"Dry run: wrote {site.out}/page-{page_id}.*, block.html" + (f", page-{home_id}.*" if home_new else ""))
        return

    # Each page is read fresh immediately before its own write and the change is built on that copy, so an edit Joe
    # saved meanwhile is kept. (WordPress has no "write only if unchanged": a save landing in the same second as the
    # write can still be lost; that gap is one request.) The read-back checks only what the pipeline owns on the page
    # (the block, the version line), so an edit elsewhere is neither undone nor mistaken for a failure.
    def read_owned(pid, owned):
        """(page, what the pipeline owns on it now); Unknown if the page can't be read."""
        try:
            pg = site.page(pid)
        except Fail as e:
            raise Unknown(f"page {pid} could not be read back ({e})")
        return pg, owned(pg)

    def write_owned(pid, build, owned, what):
        """Read fresh, build on that copy, write, read back. Returns (page read back, page it was built on).
        Fail: confirmed not written (nothing of ours there). Unknown: could not be read back."""
        fresh = site.page(pid)
        k, value, want = build(fresh)
        err = None
        try:
            site.write_page(pid, k, value)
        except Fail as e:          # the answer was lost or an error came back: the write may still have landed
            err = e
        back, now = read_owned(pid, owned)
        if now != want:
            raise Fail(f"{what} did not take" + (f" ({err})" if err else "") + f"; it reads: {visible_text(str(now))[:200]}")
        return back, fresh

    def observe_block():
        """("new" | "old" | "other" | "unknown", page or None, words for the error message)."""
        try:
            pg, now = read_owned(page_id, current_block)
        except Unknown as e:
            return "unknown", None, f"the changelog page could not be read to check ({e})"
        if now == block:   # the target first: on a re-run the old block can be the same as the new one
            return "new", pg, f"the changelog page shows {version} (checked)"
        if now == before["block"]:
            return "old", pg, "the changelog page shows the entries it had before (checked)"
        return "other", pg, "the changelog block reads something else (checked); look at it"

    def build_block(new_block, expect_now=None, remember=None):
        def build(pg):
            if expect_now is not None and current_block(pg) != expect_now:
                raise Fail("the changelog block was changed by someone else meanwhile, so it was left as it is")
            if remember is not None:
                remember["block"] = current_block(pg)   # what to put back: the block as it was just before this write
            k, v = replace_block(pg, new_block)
            return k, v, new_block
        return build

    def build_home(pg):
        v = set_version_line(pg, version, entries[0]["date"], version_class)
        if v is None:
            raise Fail("the home page's version line disappeared while the release ran")
        return "elementor", v, version_line_texts({"meta": {"_elementor_data": v}}, version_class)

    before = {"block": current_block(page)}
    try:
        try:
            back, _ = write_owned(page_id, build_block(block, remember=before), current_block, "The changelog page")
        except Fail as e:
            state, pg, words = observe_block()
            if state != "new":
                raise Fail(f"Writing the changelog page failed: {e}. Checked again: {words}. The home page was not touched.")
            back = pg   # it did land after all
        log(f"✓ Changelog page {page_id} written and read back")
        if home_new is not None:
            try:
                hb, _ = write_owned(home_id, build_home, lambda pg: version_line_texts(pg, version_class),
                                    "The home page's version line")
            except Unknown as e:
                raise Fail(f"The home page's version line was sent but {e}, so whether it shows {version} is not "
                           f"known (not checked); {observe_block()[2]}. Look at both pages, then re-run the job.")
            except Fail as e:
                # Confirmed: the home page is not on the new version. Put only the changelog block back (built on a
                # fresh read, and only if it is still the block just written), so the site never shows the new
                # version in one place and the old one in the other; other edits on the page stay.
                try:
                    write_owned(page_id, build_block(before["block"], expect_now=block), current_block,
                                "Putting the changelog block back")
                    undo = "the changelog block was put back as it was (read back)"
                except Fail as e2:
                    undo = f"putting the changelog block back was not confirmed ({e2}); now {observe_block()[2]}"
                raise Fail(f"The home page's version line could not be set: {e}. {undo}. Fix the cause and re-run the job.")
            log(f"✓ Home page {home_id} version line set to {version} and read back")
    finally:
        try:
            site.clear_cache()   # whatever is stored now is what visitors should see
        except Fail as e:
            log(f"::warning::Clearing Elementor's cache failed ({e}); Elementor > Tools > Clear Files & Data.")

    # Live: what a visitor gets. First with a unique query string (no page cache can answer: this is what the site
    # now renders), then the ordinary address a visitor uses, which the site's page cache (about 5 minutes) may still
    # answer with the old page for a while. The block must read exactly like the block written, with the same links;
    # the version line likewise.
    def live(url, extract, want, what, tries):
        seen = None
        for bust in (True, False):
            for attempt in range(tries if not bust else 6):
                code, body = site.public(url, bust=bust)
                seen = extract(body) if code == 200 else None
                if seen is not None and want(seen):
                    break
                time.sleep(10)
            else:
                where = "even with the page cache bypassed" if bust else "at its ordinary address (the page cache)"
                raise Fail(f"{what} {url} (HTTP {code}) does not show what was written {where}"
                           + (f": it reads '{visible_text(seen)[:300]}'" if seen else "")
                           + (". The data is stored; Elementor > Tools > Clear Files & Data, then look again." if bust
                              else ". The data is stored and renders right; the cache should catch up within minutes."))

    want_text, want_links = visible_text(block), links(block)
    live(back["link"], served_block, lambda g: visible_text(g) == want_text and links(g) == want_links,
         "The live changelog page", 36)
    log(f"✓ Live changelog page shows exactly the new block ({version} first, every older entry)")
    if home_new is not None:
        want = visible_text(version_line_texts(hb, version_class)[0])
        live(site.site + "/", lambda body: served_widget(body, version_class), lambda w: visible_text(w) == want,
             "The live home page", 36)   # the front page (its own link redirects to /)
        log(f"✓ Live home page reads '{want}'")


def main(argv=None):
    ap = argparse.ArgumentParser(description="CHANGELOG.md to the changelog page")
    ap.add_argument("command", choices=["check", "render", "publish"])
    ap.add_argument("version")
    ap.add_argument("--changelog", default="CHANGELOG.md")
    ap.add_argument("--prev-version")
    ap.add_argument("--prev-mac")
    ap.add_argument("--prev-win")
    ap.add_argument("--page-id", type=int, help="the changelog page (wCHANGELOG_PAGE_ID in config.cmake)")
    ap.add_argument("--plugin-name", help="wPLUGIN_NAME, for the installer titles in the media library")
    ap.add_argument("--version-class", default=VERSION_CLASS,
                    help="CSS class of the home page's version line for this product (default wla-version)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--page-json")
    ap.add_argument("--home-json")
    ap.add_argument("--media-json")
    ap.add_argument("--out", default="changelog-dry-run")
    ap.add_argument("--release-date", help="YYYY-MM-DD for <version> when its entry says (release date) and it has "
                                           "no tag yet (dry runs); otherwise the tag's date is used")
    a = ap.parse_args(argv)
    try:
        entries = parse(open(a.changelog, encoding="utf-8").read())
        if a.command == "check":
            check(entries, a.version)
            print(f"✓ CHANGELOG.md has an approved entry for {a.version}")
        elif a.command == "render":
            prev = (a.prev_version, a.prev_mac, a.prev_win) if a.prev_version else None
            resolve_dates(entries, a.version, a.release_date)
            sys.stdout.write(render(entries, a.version, prev) + "\n")
        else:
            if not a.page_id or not a.plugin_name:
                raise Fail("publish needs --page-id and --plugin-name.")
            if a.dry_run:
                if not a.page_json:
                    raise Fail("--dry-run needs --page-json (a copy of the changelog page).")
                site = FileSite(a.page_json, a.home_json, a.media_json, a.out)
            else:
                site = LiveSite()
            publish(site, entries, a.version, a.page_id, a.plugin_name, a.dry_run, a.release_date,
                    version_class=a.version_class)
    except Fail as e:
        print(f"::error::{e}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
