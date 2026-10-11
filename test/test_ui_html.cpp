// Unit tests for the served web UI asset (host side, env:native).
// Run: pio test -e native
//
// UiHtml.h is pure data (SC-001), so nothing in the firmware ever validates it
// and CI never renders it. These are contract checks on the exact string the
// browser loads - the only automated gate the HTML asset has.
#include <unity.h>

#include <UiHtml.h>

#include <FirmwareCore.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace {
bool inHtml(const char *needle) {
  return std::strstr(INDEX_HTML, needle) != nullptr;
}

// Substring count for "exactly once" contracts (e.g. one global Stop).
uint32_t countInHtml(const char *needle) {
  uint32_t count = 0;
  for (const char *p = INDEX_HTML; (p = std::strstr(p, needle)) != nullptr;
       ++p) {
    ++count;
  }
  return count;
}

// Every value of an HTML attribute like `data-cmd=` in the asset, deduplicated.
// Returns empty on a scan-pattern regression - callers must assert non-empty
// so a failed scan can never pass vacuously (#98 AC).
std::vector<std::string> htmlAttrValues(const char *attrEq) {
  std::vector<std::string> out;
  const char *p = INDEX_HTML;
  while ((p = std::strstr(p, attrEq)) != nullptr) {
    p += strlen(attrEq);  // step past the opening quote
    const char *end = std::strchr(p, '"');
    if (end == nullptr) {
      break;  // malformed asset; another contract test will flag it
    }
    out.emplace_back(p, end);
    p = end;
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
}  // namespace

void test_index_html_has_content(void) {
  TEST_ASSERT_GREATER_THAN_UINT32(1000, static_cast<uint32_t>(strlen(INDEX_HTML)));
}

// A page without lang or charset is unreadable for assistive technology and
// mis-decoded by the browser; both are one-line regressions to lose.
void test_index_html_declares_language_and_charset(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("<html lang="), "missing lang attribute");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("<meta charset=\"utf-8\">"),
                           "missing charset declaration");
}

// #59: with no icon declared the browser requests /favicon.ico on every page
// load and the ESP32 answers 404 (console error on a clean boot). The icon is
// an inline data URI, so it costs no request and no flash partition.
void test_index_html_declares_inline_favicon(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("rel=\"icon\""),
                           "no icon link: browser will request /favicon.ico (#59)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data:image/svg+xml,"),
                           "icon must be inline (data URI), not a served file");
}

// Every interactive control is wired by attribute; a renamed or dropped
// attribute silently removes the control from the operator's reach.
void test_index_html_wires_all_controls(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"en\""), "driver enable button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"off\""), "driver disable button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"stop\""), "stop button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-jog=\"up\""), "jog up button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-jog=\"down\""), "jog down button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-prefix=\"feed\""), "feed form missing");
}

// True if the tag containing `anchor` also carries an aria-label (search stops
// at the tag's closing bracket, so a label in a later tag cannot satisfy it).
static bool tagHasAriaLabel(const char *anchor) {
  const char *at = std::strstr(INDEX_HTML, anchor);
  if (at == nullptr) {
    return false;
  }
  const char *tagEnd = std::strchr(at, '>');
  const char *label = std::strstr(at, "aria-label=\"");
  return tagEnd != nullptr && label != nullptr && label < tagEnd;
}

// The jog buttons are labelled with a bare glyph (U+25B2/U+25BC): without a text
// alternative a screen reader announces only "up-pointing triangle", on a
// control that moves the crosshead (WCAG 4.1.2 name/role/value).
void test_jog_buttons_have_text_names(void) {
  TEST_ASSERT_TRUE_MESSAGE(tagHasAriaLabel("data-jog=\"up\""),
                           "jog up button has no aria-label");
  TEST_ASSERT_TRUE_MESSAGE(tagHasAriaLabel("data-jog=\"down\""),
                           "jog down button has no aria-label");
}

// Command replies and connection errors appear in #message; without a live
// region they are silent for assistive technology (WCAG 4.1.3 status messages).
void test_message_area_is_a_live_region(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("id=\"message\" aria-live=\"polite\""),
                           "#message is not announced as a status message");
}

// Keyboard parity for the hold-to-jog buttons (WCAG 2.1.1): the pointer path
// alone (pointerdown/pointerup) is unreachable from the keyboard, so a keyboard
// user cannot jog at all - and keydown/keyup plus blur is what gives the same
// release-stops-it and focus-lost-stops-it behaviour.
void test_jog_supports_keyboard_activation(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("addEventListener(\"keydown\""), "no keydown handler");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("addEventListener(\"keyup\""), "no keyup handler");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("addEventListener(\"blur\""),
                           "no blur handler: a held key would never stop");
}

// --- M2-T005: embedded locale dictionary (plan D3, FR-005) -------------------

// Extract every occurrence of `prefix + "..."` (key up to the next quote) from
// [from, to) into a sorted unique vector. With keysOnly, a token counts only
// when the closing quote is immediately followed by ':' (JS object key), so
// dictionary VALUES do not pollute the key set.
static std::vector<std::string> extractKeys(const char *from, const char *to,
                                            const std::string &prefix,
                                            bool keysOnly = false,
                                            bool wordStart = false,
                                            bool dedup = true) {
  std::vector<std::string> keys;
  for (const char *p = from; p != nullptr && p < to;) {
    p = std::strstr(p, prefix.c_str());
    if (p == nullptr || p >= to) {
      break;
    }
    const bool boundaryOk =
        !wordStart || (p > INDEX_HTML && !std::isalnum(p[-1]) && p[-1] != '_' &&
                       p[-1] != '$' && p[-1] != '.');
    p += prefix.size();
    const char *end = std::strchr(p, '"');
    if (end != nullptr && end < to) {
      const bool isKey = boundaryOk && (!keysOnly || (end + 1 < to && end[1] == ':'));
      if (isKey) {
        keys.emplace_back(p, end);
      }
      p = end;
    }
  }
  std::sort(keys.begin(), keys.end());
  if (dedup) {
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  }
  return keys;
}

static const char *dictStart() {
  return std::strstr(INDEX_HTML, "const I18N = {");
}

// Static markup default: English (SC-001 fresh boot). The visible part (up to
// the script block) must carry no German umlauts and no German UI words -
// the DE strings may only live inside the JS dictionary.
void test_static_markup_is_english(void) {
  const char *script = std::strstr(INDEX_HTML, "<script>");
  TEST_ASSERT_NOT_NULL(script);
  for (const char *p = INDEX_HTML; p < script; ++p) {
    TEST_ASSERT_TRUE_MESSAGE(static_cast<unsigned char>(*p) < 0x80,
                             "non-ASCII byte in static markup (DE leaked)");
  }
  const std::vector<std::string> german = {"Selbsttest", "Treiber", "Vorschub",
                                           "Steuerung", "setzen", "fahren",
                                           "Waegezelle", "Gewicht"};
  const std::string head(INDEX_HTML, script);
  for (const auto &word : german) {
    TEST_ASSERT_TRUE_MESSAGE(head.find(word) == std::string::npos,
                             ("German word in static markup: " + word).c_str());
  }
}

// The dictionary exists for both locales and the html default is lang="en".
void test_i18n_dictionary_has_both_locales(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("<html lang=\"en\">"),
                           "static default must be English (SC-001)");
  const char *dict = dictStart();
  TEST_ASSERT_NOT_NULL_MESSAGE(dict, "I18N dictionary missing");
  TEST_ASSERT_NOT_NULL(std::strstr(dict, "en: {"));
  TEST_ASSERT_NOT_NULL(std::strstr(dict, "de: {"));
}

// Every key the static markup asks for (text, placeholder, aria-label) and
// every dynamic t("...") key must exist in BOTH dictionaries - a missing DE
// entry silently serves English (and the EN fallback hides a missing EN key).
void test_i18n_dictionary_is_complete(void) {
  const char *dict = dictStart();
  TEST_ASSERT_NOT_NULL(dict);
  const char *en = std::strstr(dict, "en: {");
  const char *de = std::strstr(dict, "de: {");
  TEST_ASSERT_NOT_NULL(en);
  TEST_ASSERT_NOT_NULL(de);
  const std::vector<std::string> enKeys = extractKeys(en, de, "\"", true);
  const char *dictEnd = std::strstr(de, "\n    };");
  TEST_ASSERT_NOT_NULL(dictEnd);
  const std::vector<std::string> deKeys = extractKeys(de, dictEnd, "\"", true);
  TEST_ASSERT_TRUE_MESSAGE(enKeys == deKeys, "EN and DE dictionaries differ");
  TEST_ASSERT_TRUE_MESSAGE(enKeys.size() > 40, "dictionary unexpectedly small");

  std::vector<std::string> used;
  for (const char *attr :
       {"data-i18n=\"", "data-i18n-ph=\"", "data-i18n-al=\""}) {
    auto keys = extractKeys(INDEX_HTML, dict, attr);
    used.insert(used.end(), keys.begin(), keys.end());
  }
  auto dynamic =
      extractKeys(dictEnd, INDEX_HTML + strlen(INDEX_HTML), "t(\"", false,
                  true);  // real t(" calls only, not createElement("li")
  used.insert(used.end(), dynamic.begin(), dynamic.end());
  // Indirect keys: the selftest status map and the cycle-state map feed t()
  // through their values (t("ts" + state) is invisible to the literal scan).
  const std::vector<std::string> indirect = {
      "stOk", "stWarn", "stFail", "stNoConn", "stNoWired",
      "tsIdle", "tsHoming", "tsPulling", "tsTail", "tsDone", "tsLimit",
      "tsAborted", "tsError", "datumYes", "datumNo", "breakNone", "truncYes",
      "truncNo", "errCycleActive"};
  used.insert(used.end(), indirect.begin(), indirect.end());
  std::sort(used.begin(), used.end());
  used.erase(std::unique(used.begin(), used.end()), used.end());
  for (const auto &key : used) {
    TEST_ASSERT_TRUE_MESSAGE(
        std::binary_search(enKeys.begin(), enKeys.end(), key),
        ("static/dynamic key missing in EN dict: " + key).c_str());
    TEST_ASSERT_TRUE_MESSAGE(
        std::binary_search(deKeys.begin(), deKeys.end(), key),
        ("static/dynamic key missing in DE dict: " + key).c_str());
  }
}

// FR-003 switch path: the selector exists and routes through the shared
// `lang` command the serial CLI uses (single policy).
void test_language_selector_present(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-lang=\"en\""), "EN selector missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-lang=\"de\""), "DE selector missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("sendCommand(\"lang \" + btn.dataset.lang)"),
                           "selector does not send the lang command");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("applyLocale(data.locale)"),
                           "UI does not follow the server locale");
}

// M2-T006: the client keeps the last locale across reloads (localStorage,
// read before the first paint, written on every switch) and never trusts the
// cached value for more than the two known locales - the server value via
// /api/status remains the authority.
void test_locale_survives_page_reload(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("localStorage.getItem(\"locale\")"),
                           "locale not read back before first paint");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("localStorage.setItem(\"locale\", locale)"),
                           "locale not persisted on switch");
  // cached value cannot inject anything but de; anything else stays EN
  TEST_ASSERT_TRUE_MESSAGE(inHtml("localStorage.getItem(\"locale\") === \"de\""),
                           "cached locale is not validated against the known set");
}

// #98: the UI wires controls by string attribute, the firmware parses by
// string - two hand-maintained lists nothing checked against each other. A
// renamed parser command or a newly added, unwired control used to produce a
// silently dead button ("Unknown command." on the machine). Every
// `data-cmd` value must resolve through the parser's exact-name table
// (commandFromName is strcmp-exact and runs before the value prefixes, so
// prefix shadowing cannot mask an unwired name).
void test_all_data_cmd_values_are_known_commands(void) {
  const std::vector<std::string> cmds = htmlAttrValues("data-cmd=\"");
  TEST_ASSERT_TRUE_MESSAGE(!cmds.empty(),
                           "scan found no data-cmd in the asset (#98 guard)");
  for (const std::string &cmd : cmds) {
    TEST_ASSERT_TRUE_MESSAGE(commandFromName(cmd.c_str()) != Command::Unknown,
                             cmd.c_str());
  }
}

// Same cross-check for value commands. The dispatch prefixes live in
// src/Commands.cpp (Arduino-only); the naive parseCommandValue("<x> 1", "<x>")
// probe the issue proposed turned out vacuous - any string matches itself.
// So: pin the recognized prefix set here AND tie it to the pure parser side:
// every motion prefix in the list must still be claimed by isMotionCommand()
// (cal_apply is a value command but not motion). Drop a branch in either
// layer and this goes red.
void test_all_data_prefix_values_are_value_commands(void) {
  static const char *kKnown[] = {"acc", "cal_apply", "feed", "move", "rpm",
                                 "turn", "testspeed", "testarm", "testdrop"};
  static const char *kMotion[] = {"acc", "feed", "move", "rpm", "turn"};

  const std::vector<std::string> prefixes = htmlAttrValues("data-prefix=\"");
  TEST_ASSERT_TRUE_MESSAGE(!prefixes.empty(),
                           "scan found no data-prefix in the asset (#98 guard)");
  for (const std::string &prefix : prefixes) {
    const bool known = std::find(std::begin(kKnown), std::end(kKnown), prefix) !=
                       std::end(kKnown);
    TEST_ASSERT_TRUE_MESSAGE(known, prefix.c_str());
  }
  for (const char *prefix : kMotion) {
    const std::string probe = std::string(prefix) + " 1";
    TEST_ASSERT_TRUE_MESSAGE(isMotionCommand(probe.c_str()), prefix);
  }
}

// #99 (WCAG 3.3.2): the numeric inputs were named by placeholder only, and a
// placeholder vanishes exactly when the operator starts typing. Every numeric
// input needs an id AND a real, visible <label for="..."> - checked across
// ALL number inputs by scanning the tags, so a 6th input cannot escape.
// Visibility/contrast is a rendering property; the markup carries no
// display:none for labels (asserted) and the CSS uses the page's primary
// text color - confirmed on the bench (see PR).
void test_number_inputs_have_visible_labels(void) {
  uint32_t count = 0;
  const char *p = INDEX_HTML;
  while ((p = std::strstr(p, "<input")) != nullptr) {
    const char *end = std::strchr(p, '>');
    if (end == nullptr) {
      break;
    }
    const std::string tag(p, static_cast<size_t>(end - p));
    p = end;
    if (tag.find("type=\"number\"") == std::string::npos) {
      continue;
    }
    ++count;
    const std::string idKey = "id=\"";
    const size_t idPos = tag.find(idKey);
    TEST_ASSERT_TRUE_MESSAGE(idPos != std::string::npos, "numeric input without id");
    const size_t idEnd = tag.find('"', idPos + idKey.size());
    const std::string id = tag.substr(idPos + idKey.size(), idEnd - idPos - idKey.size());
    const std::string labelFor = "<label for=\"" + id + "\"";
    TEST_ASSERT_TRUE_MESSAGE(inHtml(labelFor.c_str()),
                             "numeric input lacks a <label for>");
  }
  // guard: the five known numeric inputs exist; a scan regression must not
  // pass vacuously (#98 pattern).
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(5, count,
                                              "numeric input scan found nothing");
  // labels must not be hidden by CSS (AC: visible on screen).
  TEST_ASSERT_TRUE_MESSAGE(inHtml("label {"), "no label style block");
  TEST_ASSERT_TRUE_MESSAGE(!inHtml("label { display:none") &&
                               !inHtml("label { display: none"),
                           "labels are hidden - WCAG 3.3.2 violation");
}

// #99 development finding: JS silently lets the LAST duplicate object key
// win - a new input-label key "lblFeed" was overwritten by the status pill's
// "lblFeed" and the label rendered the pill's text. Duplicate dictionary keys
// are a hard error, in both locales.
void test_i18n_dictionary_has_no_duplicate_keys(void) {
  const char *dict = dictStart();
  TEST_ASSERT_NOT_NULL(dict);
  const char *en = std::strstr(dict, "en: {");
  const char *de = std::strstr(dict, "de: {");
  TEST_ASSERT_NOT_NULL(en);
  TEST_ASSERT_NOT_NULL(de);
  const char *dictEnd = std::strstr(de, "\n    };");
  TEST_ASSERT_NOT_NULL(dictEnd);
  std::vector<std::string> enRaw =
      extractKeys(en, de, "\"", true, false, false);
  std::vector<std::string> deRaw =
      extractKeys(de, dictEnd, "\"", true, false, false);
  // sorted by extractKeys, so std::unique leaves exactly one of each run
  const size_t enUnique = std::unique(enRaw.begin(), enRaw.end()) - enRaw.begin();
  const size_t deUnique = std::unique(deRaw.begin(), deRaw.end()) - deRaw.begin();
  TEST_ASSERT_EQUAL_UINT_MESSAGE(enRaw.size(), enUnique,
                                 "duplicate key in EN dictionary");
  TEST_ASSERT_EQUAL_UINT_MESSAGE(deRaw.size(), deUnique,
                                 "duplicate key in DE dictionary");
}

// --- M7 US3/FR-005..008: tear workflow surface --------------------------------

// FR-005: Home, Start test and the operator-paced sample prompt are UI
// elements; the buttons ride the shared data-cmd wiring (and are therefore
// also covered by the known-command scan above).
void test_tear_controls_present(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"home\""),
                           "home button missing (FR-005)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"test\""),
                           "start-test button missing (FR-005)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("id=\"samplePrompt\""),
                           "sample-insertion prompt missing (FR-005)");
}

// Taste probe C1-analog (#167): Stop used to render twice (jog row + axis
// card). The redesign keeps ONE global, always-visible Stop - more would
// blur which one is the emergency control.
void test_single_global_stop(void) {
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(
      1, countInHtml("data-cmd=\"stop\""),
      "Stop must exist exactly once (one global emergency control)");
}

// FR-006/FR-007/FR-008: the run curve renders from the sample buffer and
// the CSV downloads as a file - both from the /api/data endpoint added for
// M7; the summary block carries the testdata facts.
void test_csv_download_and_run_curve_wired(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("href=\"/api/data\""),
                           "CSV link does not target /api/data (FR-007)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("download=\"tensile-run.csv\""),
                           "CSV link lacks the download attribute (FR-007)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("id=\"runChart\""),
                           "run curve canvas missing (FR-006)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("id=\"runSummary\""),
                           "run summary block missing (FR-008)");
}

// US2 owner decision 2026-10-11 (#169): monochrome + exactly ONE accent -
// red for Stop/danger. The retired hue families (green success, rust
// accent pair, blue favicon/plot) must not survive the redesign.
void test_visual_language_mono_single_accent(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("--danger:"),
                           "the single red accent variable is missing (#169)");
  TEST_ASSERT_TRUE_MESSAGE(!inHtml("--success"),
                           "a green accent variable exists: one accent only (#169)");
  TEST_ASSERT_TRUE_MESSAGE(!inHtml("--accent"),
                           "a second accent variable exists: one accent only (#169)");
  TEST_ASSERT_TRUE_MESSAGE(!inHtml("#2b8bd9") && !inHtml("%232b8bd9"),
                           "blue hue survives (favicon/plot line, taste gate 5)");
  TEST_ASSERT_TRUE_MESSAGE(!inHtml("#2f6f4f"), "green hue survives (US2)");
  TEST_ASSERT_TRUE_MESSAGE(!inHtml("#b14d25") && !inHtml("#8f3816"),
                           "rust hues survive (US2)");
}

// Taste gates N9 (one radius scale: 8 px + 999 px pill for toggles) and 4
// (respect prefers-reduced-motion on the jog/press transforms).
void test_one_radius_scale_and_reduced_motion(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("border-radius: 8px"),
                           "base radius 8px missing (taste N9)");
  static const char *kOldRadii[] = {
      "border-radius: 12px", "border-radius: 14px", "border-radius: 16px",
      "border-radius: 18px", "border-radius: 20px"};
  for (const char *r : kOldRadii) {
    TEST_ASSERT_TRUE_MESSAGE(!inHtml(r),
                             ("retired radius value survived: " + std::string(r))
                                 .c_str());
  }
  TEST_ASSERT_TRUE_MESSAGE(
      inHtml("@media (prefers-reduced-motion: reduce)"),
      "reduced-motion guard missing (taste gate 4)");
}

// Taste gate T6: the status register mixed cases ("OK" vs "warning").
// EN display words are capitalized-first, uniformly.
void test_status_words_case_unified(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("\"stWarn\": \"Warning\""),
                           "EN status words must be case-unified (taste T6)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("\"stFail\": \"Fault\""),
                           "EN status words must be case-unified (taste T6)");
}

// The tear panel exposes the M3 test configuration as value forms (same
// prefixes as the serial grammar; clamped by the firmware).
void test_tear_config_forms_present(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-prefix=\"testspeed\""),
                           "testspeed form missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-prefix=\"testarm\""),
                           "testarm form missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-prefix=\"testdrop\""),
                           "testdrop form missing");
}

// #95: stale/offline handling + poll backoff contract on the served page.
void test_ui_status_poll_uses_backoff_not_fixed_interval(void) {
  // The fixed 100 ms interval is gone; a backoff ladder reschedules itself.
  TEST_ASSERT_EQUAL_UINT32(0, countInHtml("setInterval(refreshStatus"));
  TEST_ASSERT_TRUE(inHtml("POLL_STEPS_MS"));
}

void test_ui_has_stale_banner_hooks(void) {
  TEST_ASSERT_TRUE(inHtml("id=\"staleBanner\""));
  TEST_ASSERT_TRUE(inHtml("markStale"));
  TEST_ASSERT_TRUE(inHtml("clearStale"));
  TEST_ASSERT_TRUE(inHtml("staleContact"));
}

void test_ui_detects_device_reboot_via_uptime(void) {
  TEST_ASSERT_TRUE(inHtml("uptimeMs"));
  TEST_ASSERT_TRUE(inHtml("deviceRebooted"));
}

void test_ui_status_fetch_has_timeout(void) {
  // Bench-found (reboot under an open page): a silently-dead TCP
  // connection (ESP reboot sends no FIN/RST) hangs fetch() forever -
  // polling must abort and fall into the stale/backoff path.
  TEST_ASSERT_TRUE(inHtml("AbortSignal.timeout"));
}

void test_ui_reboot_announce_is_latched(void) {
  // Bench-found: the message-line announce was transient - poll noise
  // during AP recovery overwrote it within seconds. A reboot must stay
  // visible until the operator acts (sendCommand clears it).
  TEST_ASSERT_TRUE(inHtml("id=\"rebootBanner\""));
}

// #95 bench pass, vision+DOM cross-check: the selftest check rows were
// cached by the locale-NEUTRAL checks JSON, so a locale switch left the
// rows in the old language forever (only the overall pill updated).
void test_ui_selftest_rows_rerender_on_locale_switch(void) {
  TEST_ASSERT_EQUAL_UINT32(0, countInHtml("lastSelftestChecksJson"));
  TEST_ASSERT_TRUE(inHtml("lastSelftestChecksKey"));
}

// Layout contract (operator request 2026-10-11): "Tensile test" and
// "Run curve" share one 50/50 row on desktop, stacking below 640 px.
void test_ui_test_and_runcurve_share_a_row(void) {
  TEST_ASSERT_TRUE(inHtml("class=\"row2\""));
  TEST_ASSERT_TRUE(inHtml(".row2 .plot-card"));
}

// Operator request 2026-10-11: brand title is "JanTec ATLAS" in both
// locales (was "Zugversuch Steuerung" / "Tensile Test Control").
void test_ui_h1_is_jantec_atlas(void) {
  // 3 = EN dict + DE dict + the static markup default (ships English).
  TEST_ASSERT_EQUAL_UINT32(3, countInHtml("JanTec ATLAS"));
  TEST_ASSERT_EQUAL_UINT32(0, countInHtml("Zugversuch Steuerung"));
  TEST_ASSERT_EQUAL_UINT32(0, countInHtml("Tensile Test Control"));
}

// Home | Start test | Download CSV share one 3-column action row (the
// CSV link left its lone note paragraph); every btnrow keeps a margin
// to the row above.
void test_ui_csv_button_in_action_row(void) {
  TEST_ASSERT_EQUAL_UINT32(1, countInHtml("id=\"csvDownload\""));
  TEST_ASSERT_TRUE(inHtml("btnrow btnrow3"));
  TEST_ASSERT_TRUE(inHtml(".btnrow3"));
  TEST_ASSERT_EQUAL_UINT32(0, countInHtml("p class=\"note\"><a class=\"btn\""));
}

// Operator request 2026-10-11: the action row sits BELOW the run
// summary pills (Max Kraft / Bruchweg / Messwerte), not between the
// sample prompt and the summary.
void test_ui_action_row_below_run_summary(void) {
  const char *summary = std::strstr(INDEX_HTML, "id=\"runSummary\"");
  const char *actions = std::strstr(INDEX_HTML, "btnrow btnrow3");
  TEST_ASSERT_NOT_NULL(summary);
  TEST_ASSERT_NOT_NULL(actions);
  TEST_ASSERT_TRUE(summary < actions);
}

void runUiHtmlTests(void) {
  RUN_TEST(test_index_html_has_content);
  RUN_TEST(test_index_html_declares_language_and_charset);
  RUN_TEST(test_index_html_declares_inline_favicon);
  RUN_TEST(test_index_html_wires_all_controls);
  RUN_TEST(test_jog_buttons_have_text_names);
  RUN_TEST(test_message_area_is_a_live_region);
  RUN_TEST(test_jog_supports_keyboard_activation);
  RUN_TEST(test_static_markup_is_english);
  RUN_TEST(test_ui_status_poll_uses_backoff_not_fixed_interval);
  RUN_TEST(test_ui_has_stale_banner_hooks);
  RUN_TEST(test_ui_detects_device_reboot_via_uptime);
  RUN_TEST(test_ui_status_fetch_has_timeout);
  RUN_TEST(test_ui_reboot_announce_is_latched);
  RUN_TEST(test_ui_selftest_rows_rerender_on_locale_switch);
  RUN_TEST(test_ui_test_and_runcurve_share_a_row);
  RUN_TEST(test_ui_h1_is_jantec_atlas);
  RUN_TEST(test_ui_csv_button_in_action_row);
  RUN_TEST(test_ui_action_row_below_run_summary);
  RUN_TEST(test_i18n_dictionary_has_both_locales);
  RUN_TEST(test_i18n_dictionary_is_complete);
  RUN_TEST(test_all_data_cmd_values_are_known_commands);
  RUN_TEST(test_all_data_prefix_values_are_value_commands);
  RUN_TEST(test_language_selector_present);
  RUN_TEST(test_locale_survives_page_reload);
  RUN_TEST(test_number_inputs_have_visible_labels);
  RUN_TEST(test_i18n_dictionary_has_no_duplicate_keys);
  RUN_TEST(test_tear_controls_present);
  RUN_TEST(test_single_global_stop);
  RUN_TEST(test_csv_download_and_run_curve_wired);
  RUN_TEST(test_visual_language_mono_single_accent);
  RUN_TEST(test_one_radius_scale_and_reduced_motion);
  RUN_TEST(test_status_words_case_unified);
  RUN_TEST(test_tear_config_forms_present);
}
