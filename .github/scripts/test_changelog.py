#!/usr/bin/env python3
# test_changelog.py: tests for changelog.py and its place in the release pipeline, with a fake site (no network)
# ==========================================================================================
# [Claude 2026-10-30] The changelog step only runs on a real v* tag, the worst moment to find a bug in it.
# Run: python3 .github/scripts/test_changelog.py     (CI runs it in preflight; needs PyYAML for the workflow checks)
# ==========================================================================================
import copy
import datetime
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import changelog as cl  # noqa: E402

cl.time.sleep = lambda s: None   # the live checks retry with a pause; not in tests

LOG = """# Plugin changelog

<!-- a note for maintainers -->

## 1.1.0 (release date)
**Mac**
- Runs on <Apple Silicon> & Intel.
- A [link](https://example.com/x?a=1&b=2) and **bold**,
  continued on the next line.

## 1.0.2 (2026-01-28)
- Windows: no runtime needed.

## 1.0.1 (2026-01-26)
First release with a fix.
- One fix.
"""

MEDIA = [
    {"title": {"rendered": "Plug Installer v1.0.2 for Mac"}, "source_url": "https://s/u/PlugMacInstaller_1.0.2.pkg", "date": "2026-01-28T00:00:00"},
    {"title": {"rendered": "Plug Installer v1.0.2 for Windows"}, "source_url": "https://s/u/PlugWinInstaller_1.0.2.exe", "date": "2026-01-28T00:00:00"},
]


def page_data(block_html="<p>old</p>", classes="wla-changelog"):
    return [{"id": "c1", "elType": "container", "settings": {"_title": "Page"}, "elements": [
        {"id": "h1", "elType": "widget", "widgetType": "heading", "settings": {"title": "Changelog"}, "elements": []},
        {"id": "i1", "elType": "widget", "widgetType": "text-editor", "settings": {"editor": "<p>Joe's intro</p>"}, "elements": []},
        {"id": "b1", "elType": "widget", "widgetType": "text-editor",
         "settings": {"editor": block_html, "_css_classes": classes, "custom_css": "selector h4{}"}, "elements": []},
        {"id": "x1", "elType": "widget", "widgetType": "text-editor", "settings": {"editor": "<p>Joe's footnote</p>"}, "elements": []},
    ]}]


def page(pid=10, data=None, status="publish"):
    return {"id": pid, "status": status, "link": f"https://s/p{pid}/",
            "meta": {"_elementor_edit_mode": "builder",
                     "_elementor_data": json.dumps(page_data() if data is None else data)},
            "content": {"raw": ""}}


def home(text="<p>1.0.2, released 28 January 2026</p>"):
    data = [{"id": "s1", "elType": "container", "settings": {}, "elements": [
        {"id": "v1", "elType": "widget", "widgetType": "text-editor", "settings": {"editor": text, "_css_classes": "wla-version"}, "elements": []},
        {"id": "o1", "elType": "widget", "widgetType": "text-editor", "settings": {"editor": "<p>Other 9.9.9</p>"}, "elements": []},
    ]}]
    return {"id": 20, "status": "publish", "link": "https://s/home/", "meta": {"_elementor_edit_mode": "builder", "_elementor_data": json.dumps(data)}}


class FakeLive:
    """Stands in for LiveSite: in-memory pages; the public page is rendered from the stored data unless told not to."""
    site = "https://s"

    def __init__(self, pages, front=20, media=MEDIA, stale=False, mangle=False, dead=(), fail_write=None, wrong_bullets=False):
        self.pages = {p["id"]: copy.deepcopy(p) for p in pages}
        self.front, self.media, self.stale, self.mangle, self.dead = front, media, stale, mangle, dead
        self.fail_write, self.wrong_bullets = fail_write, wrong_bullets
        self.writes, self.cleared = [], 0
        self.served = {pid: self.render(pid) for pid in self.pages}

    def render(self, pid):
        # as Elementor renders it: one div per element, its CSS classes after its own, WordPress's curly apostrophes
        return "<html>" + "".join(
            f'<div class="elementor-element elementor-element-{el["id"]} {" ".join(cl.classes(el))} elementor-widget">'
            f'<div class="elementor-widget-container">{el["settings"].get("editor", "").replace(chr(39), "&#8217;")}</div></div>'
            for el in self.walk(json.loads(self.pages[pid]["meta"]["_elementor_data"]))) + "</html>"

    def walk(self, els):
        for el in els:
            yield el
            yield from self.walk(el.get("elements", []))

    def page(self, pid):
        return copy.deepcopy(self.pages[pid])

    def front_page_id(self):
        return self.front

    def media_url(self, title):
        hits = [m for m in self.media if m["title"]["rendered"] == title]
        return hits[0]["source_url"] if hits else None

    def write_page(self, pid, kind, value):
        if pid == self.fail_write:
            raise cl.Fail(f"POST page {pid} answered HTTP 500")
        self.writes.append((pid, kind))
        self.pages[pid]["meta"]["_elementor_data"] = value if not self.mangle else value.replace("1.1.0", "1.1.O")

    def clear_cache(self):
        self.cleared += 1
        if not self.stale:
            self.served = {pid: self.render(pid) for pid in self.pages}
        if self.wrong_bullets:   # headings right, a bullet wrong: must not pass
            self.served[10] = self.served[10].replace("Runs on", "Ran on")

    def public(self, url):
        pid = 20 if url == self.site + "/" else next(p for p in self.pages if self.pages[p]["link"] == url)
        return 200, self.served[pid]

    def reachable(self, url):
        return url not in self.dead


def entries(text=LOG, approve=True):
    return cl.parse(text if not approve else text)


def lookup(dates):
    return lambda v, current=False: dates.get(v)


class Parse(unittest.TestCase):
    def test_entries(self):
        e = cl.parse(LOG)
        self.assertEqual([x["version"] for x in e], ["1.1.0", "1.0.2", "1.0.1"])
        self.assertIsNone(e[0]["date"])
        self.assertEqual(e[0]["items"][0], ["group", "Mac"])
        self.assertIn("continued on the next line", e[0]["items"][2][1])
        self.assertEqual(e[2]["items"][0], ["text", "First release with a fix."])

    def test_bad_heading(self):
        for bad in ["## 1.1.0 (not released yet)\n- x\n", "## 1.1 (2026-01-01)\n- x\n", "## 1.1.0 (2026-02-30)\n- x\n",
                    "## 1.1.0 (2026-01-01)\n### Sub\n- x\n"]:
            with self.assertRaises(cl.Fail, msg=bad):
                cl.parse(bad)

    def test_order_and_duplicates(self):
        with self.assertRaises(cl.Fail):
            cl.parse("## 1.0.1 (2026-01-01)\n- a\n## 1.0.2 (2026-01-02)\n- b\n")
        with self.assertRaises(cl.Fail):
            cl.parse("## 1.0.1 (2026-01-01)\n- a\n## 1.0.1 (2026-01-02)\n- b\n")

    def test_check(self):
        e = cl.parse(LOG)
        cl.check(e, "1.1.0")
        with self.assertRaisesRegex(cl.Fail, "no entry"):
            cl.check(e, "1.2.0")
        with self.assertRaisesRegex(cl.Fail, "newest"):
            cl.check(e, "1.0.2")
        with self.assertRaisesRegex(cl.Fail, "draft"):
            cl.check(cl.parse(LOG.replace("**Mac**", "<!-- draft: Joe approves -->\n**Mac**")), "1.1.0")
        with self.assertRaisesRegex(cl.Fail, "no bullet"):
            cl.check(cl.parse("## 1.0.0 (2025-01-01)\nJust words.\n"), "1.0.0")

    def test_real_changelog_parses(self):
        e = cl.parse(open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8").read())
        self.assertTrue(e)


class Dates(unittest.TestCase):
    def test_placeholder_takes_tag_date_written_date_wins(self):
        e = cl.resolve_dates(cl.parse(LOG), "1.1.0", lookup=lookup({"1.1.0": "2026-10-30", "1.0.2": "1999-01-01"}))
        self.assertEqual(e[0]["date"], "2026-10-30")
        self.assertEqual(e[1]["date"], "2026-01-28")   # written in the heading: kept

    def test_release_date_override_and_missing_tag(self):
        e = cl.resolve_dates(cl.parse(LOG), "1.1.0", "2026-11-02", lookup({}))
        self.assertEqual(e[0]["date"], "2026-11-02")
        with self.assertRaisesRegex(cl.Fail, "no v1.1.0 tag"):
            cl.resolve_dates(cl.parse(LOG), "1.1.0", lookup=lookup({}))

    def test_new_york_day(self):
        utc = datetime.timezone.utc
        cases = [("2026-10-31T02:30:00", "2026-10-30"), ("2026-10-31T04:30:00", "2026-10-31"),
                 ("2026-01-15T04:59:00", "2026-01-14"), ("2026-01-15T05:01:00", "2026-01-15")]
        for t, want in cases:
            self.assertEqual(cl.site_day(datetime.datetime.fromisoformat(t).replace(tzinfo=utc)).isoformat(), want, t)

    def test_fallback_rule_matches_zoneinfo(self):
        try:
            from zoneinfo import ZoneInfo
            ZoneInfo(cl.SITE_TZ)
        except Exception:
            self.skipTest("no time zone data here")
        real = cl.site_day
        start = datetime.datetime(2026, 1, 1, tzinfo=datetime.timezone.utc)
        import builtins
        orig_import = builtins.__import__

        def no_zoneinfo(name, *a, **k):
            if name == "zoneinfo":
                raise ImportError
            return orig_import(name, *a, **k)
        for h in range(0, 24 * 366, 7):
            t = start + datetime.timedelta(hours=h)
            want = real(t)
            builtins.__import__ = no_zoneinfo
            try:
                got = cl.site_day(t)
            finally:
                builtins.__import__ = orig_import
            self.assertEqual(got, want, t)


class Render(unittest.TestCase):
    def setUp(self):
        self.e = cl.resolve_dates(cl.parse(LOG), "1.1.0", "2026-10-30", lookup({}))

    def test_html(self):
        h = cl.render(self.e, "1.1.0", ("1.0.2", "https://s/m.pkg", "https://s/w.exe"))
        self.assertTrue(h.startswith(cl.START) and h.endswith(cl.END))
        self.assertIn('<h4 class="wla-changelog-version">1.1.0, 30 October 2026</h4>', h)
        self.assertIn("Runs on &lt;Apple Silicon&gt; &amp; Intel.", h)
        self.assertIn('<a href="https://example.com/x?a=1&amp;b=2">link</a>', h)
        self.assertIn("<strong>bold</strong>", h)
        self.assertIn('<p class="wla-changelog-group"><strong>Mac</strong></p>', h)
        self.assertEqual(h.count("Need the previous version? 1.0.2 for"), 1)
        self.assertLess(h.index("Need the previous"), h.index("1.0.2, 28 January 2026"))   # under the newest entry
        self.assertLess(h.index("1.1.0, 30"), h.index("1.0.2, 28"))
        self.assertLess(h.index("1.0.2, 28"), h.index("1.0.1, 26"))
        self.assertNotIn("maintainers", h)
        self.assertNotIn("—", h)

    def test_no_previous_line(self):
        self.assertNotIn("Need the previous", cl.render(self.e, "1.1.0", None))

    def test_up_to_version(self):
        h = cl.render(self.e, "1.0.2")
        self.assertNotIn("1.1.0", h)
        self.assertIn("1.0.1, 26 January 2026", h)


class Block(unittest.TestCase):
    def test_only_the_block_changes(self):
        p = page()
        kind, new = cl.replace_block(p, "NEW")
        self.assertEqual(kind, "elementor")
        diffs = cl.diff_paths(json.loads(p["meta"]["_elementor_data"]), json.loads(new))
        self.assertEqual(diffs, [(0, "elements", 2, "settings", "editor")])

    def test_needs_exactly_one_block(self):
        for data in [page_data(classes="other"), page_data() + page_data()]:
            with self.assertRaisesRegex(cl.Fail, "exactly one"):
                cl.replace_block(page(data=data), "NEW")
        d = page_data()
        d[0]["elements"][2]["widgetType"] = "html"
        with self.assertRaisesRegex(cl.Fail, "not a Text Editor"):
            cl.replace_block(page(data=d), "NEW")

    def test_plain_wordpress_page(self):
        p = {"meta": {}, "content": {"raw": f"<p>Joe</p>{cl.START}old{cl.END}<p>after</p>"}}
        self.assertEqual(cl.replace_block(p, cl.START + "NEW" + cl.END),
                         ("content", f"<p>Joe</p>{cl.START}NEW{cl.END}<p>after</p>"))
        with self.assertRaises(cl.Fail):
            cl.replace_block({"meta": {}, "content": {"raw": "<p>no markers</p>"}}, "x")


class VersionLine(unittest.TestCase):
    def line(self, h):
        return cl.version_line_texts({"meta": {"_elementor_data": h}})[0]

    def test_version_and_date(self):
        self.assertEqual(self.line(cl.set_version_line(home(), "1.1.0", "2026-10-30")), "<p>1.1.0, released 30 October 2026</p>")
        self.assertEqual(self.line(cl.set_version_line(home("<p>Version 1.0.2, released January 28, 2026</p>"), "1.1.0", "2026-10-30")),
                         "<p>Version 1.1.0, released October 30, 2026</p>")
        self.assertEqual(self.line(cl.set_version_line(home("<p>1.0.2</p>"), "1.1.0", "2026-10-30")), "<p>1.1.0</p>")

    def test_only_the_line_changes(self):
        h = home()
        new = cl.set_version_line(h, "1.1.0", "2026-10-30")
        self.assertEqual(cl.diff_paths(json.loads(h["meta"]["_elementor_data"]), json.loads(new)),
                         [(0, "elements", 0, "settings", "editor")])

    def test_no_line_or_ambiguous(self):
        self.assertIsNone(cl.set_version_line({"meta": {"_elementor_data": json.dumps(page_data())}}, "1.1.0"))
        with self.assertRaisesRegex(cl.Fail, "exactly one"):
            cl.set_version_line(home("<p>1.0.2 or 1.0.1</p>"), "1.1.0")


class Publish(unittest.TestCase):
    def approved(self):
        return cl.parse(LOG)

    def run_publish(self, site, **k):
        return cl.publish(site, self.approved(), "1.1.0", 10, "Plug", lookup=lookup({"1.1.0": "2026-10-30"}), **k)

    def test_happy_path(self):
        s = FakeLive([page(), home()])
        self.run_publish(s)
        self.assertEqual(s.writes, [(10, "elementor"), (20, "elementor")])
        self.assertEqual(s.cleared, 1)
        body = s.served[10]
        self.assertIn("1.1.0, 30 October 2026", body)
        self.assertIn("https://s/u/PlugMacInstaller_1.0.2.pkg", body)
        self.assertIn("Joe's intro", cl.visible_text(body))
        self.assertIn("Joe's footnote", cl.visible_text(body))
        self.assertIn("1.1.0, released 30 October 2026", s.served[20])

    def test_refuses_draft_before_touching_anything(self):
        s = FakeLive([page(), home()])
        with self.assertRaisesRegex(cl.Fail, "draft"):
            cl.publish(s, cl.parse(LOG.replace("**Mac**", "<!-- draft -->\n**Mac**")), "1.1.0", 10, "Plug",
                       lookup=lookup({"1.1.0": "2026-10-30"}))
        self.assertEqual(s.writes, [])

    def test_unpublished_page_or_missing_block_changes_nothing(self):
        s = FakeLive([page(status="draft"), home()])
        with self.assertRaisesRegex(cl.Fail, "not published"):
            self.run_publish(s)
        s = FakeLive([page(data=page_data(classes="")), home()])
        with self.assertRaises(cl.Fail):
            self.run_publish(s)
        self.assertEqual(s.writes, [])

    def test_no_previous_installers_no_line(self):
        s = FakeLive([page(), home()], media=[])
        self.run_publish(s)
        self.assertNotIn("Need the previous", s.served[10])

    def test_dead_previous_installer_stops(self):
        s = FakeLive([page(), home()], dead=("https://s/u/PlugWinInstaller_1.0.2.exe",))
        with self.assertRaisesRegex(cl.Fail, "does not download"):
            self.run_publish(s)
        self.assertEqual(s.writes, [])

    def test_read_back_mismatch_fails(self):
        with self.assertRaisesRegex(cl.Fail, "reads back"):
            self.run_publish(FakeLive([page(), home()], mangle=True))

    def test_stale_live_page_fails(self):
        with self.assertRaisesRegex(cl.Fail, "live changelog page"):
            self.run_publish(FakeLive([page(), home()], stale=True))

    def test_older_draft_entry_is_never_published(self):
        s = FakeLive([page(), home()])
        with self.assertRaisesRegex(cl.Fail, "1.0.2"):
            cl.publish(s, cl.parse(LOG.replace("- Windows: no runtime", "<!-- draft -->\n- Windows: no runtime")),
                       "1.1.0", 10, "Plug", lookup=lookup({"1.1.0": "2026-10-30"}))
        self.assertEqual(s.writes, [])

    def test_older_placeholder_keeps_its_published_date(self):
        # 1.1.0 went out as "(release date)" on 30 October; at 1.2.0 a lightweight tag lookup would say 28 October
        published = cl.render(cl.resolve_dates(cl.parse(LOG), "1.1.0", "2026-10-30", lookup({})), "1.1.0")
        text = "## 1.2.0 (release date)\n- New.\n\n" + LOG[LOG.index("## 1.1.0"):]
        s = FakeLive([page(data=page_data(published)), home("<p>1.1.0, released 30 October 2026</p>")])
        cl.publish(s, cl.parse(text), "1.2.0", 10, "Plug", lookup=lookup({"1.2.0": "2026-12-01", "1.1.0": "2026-10-28"}))
        self.assertIn("1.1.0, 30 October 2026", s.served[10])
        self.assertIn("1.2.0, 1 December 2026", s.served[10])

    def test_edit_made_meanwhile_is_kept(self):
        s = FakeLive([page(), home()])
        real_page = s.page
        calls = {"n": 0}

        def page_then_joe_edits(pid):
            calls["n"] += pid == 10
            if pid == 10 and calls["n"] == 2:   # between the first read and the write, Joe saves in Elementor
                d = json.loads(s.pages[10]["meta"]["_elementor_data"])
                d[0]["elements"][1]["settings"]["editor"] = "<p>Joe's new intro</p>"
                s.pages[10]["meta"]["_elementor_data"] = json.dumps(d)
                s.pages[10]["modified"] = "later"
            return real_page(pid)
        s.page = page_then_joe_edits
        self.run_publish(s)
        self.assertIn("Joe's new intro", cl.visible_text(s.served[10]))
        self.assertIn("1.1.0, 30 October 2026", s.served[10])

    def test_wrong_bullet_live_fails(self):
        with self.assertRaisesRegex(cl.Fail, "does not show what was written"):
            self.run_publish(FakeLive([page(), home()], wrong_bullets=True))

    def test_home_write_fails_changelog_put_back(self):
        s = FakeLive([page(), home()], fail_write=20)
        before = s.pages[10]["meta"]["_elementor_data"]
        with self.assertRaisesRegex(cl.Fail, "put back as it was"):
            self.run_publish(s)
        self.assertEqual(json.loads(s.pages[10]["meta"]["_elementor_data"]), json.loads(before))
        self.assertNotIn("1.1.0", s.served[10])

    def test_retry_keeps_first_publication_date(self):
        s = FakeLive([page(), home()])
        self.run_publish(s)
        # the same version again the next day (a lightweight tag: "today" has moved on)
        cl.publish(s, cl.parse(LOG), "1.1.0", 10, "Plug", lookup=lookup({"1.1.0": "2026-10-31"}))
        self.assertIn("1.1.0, 30 October 2026", s.served[10])
        self.assertIn("1.1.0, released 30 October 2026", s.served[20])

    def test_two_version_lines_refused(self):
        h = home()
        d = json.loads(h["meta"]["_elementor_data"])
        d[0]["elements"][1]["settings"]["_css_classes"] = "wla-version"
        d[0]["elements"][1]["settings"]["editor"] = "<p>1.0.2</p>"
        h["meta"]["_elementor_data"] = json.dumps(d)
        with self.assertRaisesRegex(cl.Fail, "exactly one Text Editor"):
            cl.set_version_line(h, "1.1.0")

    def test_other_product_class_leaves_home_alone(self):
        s = FakeLive([page(), home()])
        cl.publish(s, cl.parse(LOG), "1.1.0", 10, "Plug", lookup=lookup({"1.1.0": "2026-10-30"}),
                   version_class="wla-version-otherplugin")
        self.assertEqual(s.writes, [(10, "elementor")])

    def test_home_without_version_line(self):
        h = home()
        h["meta"]["_elementor_data"] = json.dumps([])
        s = FakeLive([page(), h])
        self.run_publish(s)
        self.assertEqual(s.writes, [(10, "elementor")])


class Workflow(unittest.TestCase):
    def setUp(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        self.wf = yaml.safe_load(open(os.path.join(ROOT, ".github", "workflows", "workflow.yml"), encoding="utf-8"))

    def steps(self, job):
        return self.wf["jobs"][job]["steps"]

    def test_upload_titles_match(self):
        up = next(s for s in self.steps("publish") if s["name"] == "Upload Installers to WordPress")["run"]
        for plat in ("Mac", "Windows"):
            self.assertIn('"${wPLUGIN_NAME} Installer v${wPROJECT_VERSION} for %s"' % plat, up)
            self.assertEqual(cl.installer_title("${wPLUGIN_NAME}", "${wPROJECT_VERSION}", plat),
                             "${wPLUGIN_NAME} Installer v${wPROJECT_VERSION} for %s" % plat)

    def test_order_in_publish(self):
        names = [s["name"] for s in self.steps("publish")]
        i = names.index("Publish the Changelog Page and the Home Page Version Line")
        self.assertLess(names.index("Upload Installers to WordPress"), i)
        self.assertLess(names.index("Remove Older Copies of This Version from WordPress"), i)
        self.assertLess(i, names.index("Set the WooCommerce Software Version to the Tag Version"))
        self.assertTrue(self.wf["jobs"]["publish"]["if"].startswith("startsWith(github.ref, 'refs/tags/v')"))
        self.assertEqual(self.steps("publish")[0]["with"]["fetch-depth"], 0)

    def test_old_versions_never_deleted(self):
        # Only copies with this version's exact title are removed (older versions stay for "Need the previous version?")
        up = next(s for s in self.steps("publish") if s["name"] == "Upload Installers to WordPress")["run"]
        self.assertIn("select(.title.rendered == $t)", up)
        rm = next(s for s in self.steps("publish") if s["name"] == "Remove Older Copies of This Version from WordPress")["run"]
        self.assertIn("for id in $MAC_OLD_IDS $WIN_OLD_IDS", rm)

    def test_preflight_checks_entry(self):
        names = [s["name"] for s in self.steps("preflight")]
        self.assertIn("Check CHANGELOG.md Has an Approved Entry for This Version", names)


if __name__ == "__main__":
    unittest.main(verbosity=1)
