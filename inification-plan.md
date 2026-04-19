# Inification Plan

Switch config backend from toml11 to `wxFileConfig` (INI). Keep the `Value`
proxy API and every call site behaviour identical. Result: no external
toml dependency, no UTF-8 bridging, everything flows in `wxString`.

---

## Goals

- Replace toml11 with `wxFileConfig` as the backing store for every
  category (config, locale, theme, shortcuts, keywords, layout).
- Keep `Value` as the sole read/write proxy. Its public API (`at`,
  `operator[]`, `as<T>`, `value_or`, `get_or`, `asArray`, `operator=`)
  stays identical so call sites in UIManager, settings panels, Editor,
  etc. do not change.
- Drop all UTF-8 marshalling (`FromUTF8`, `utf8_str`,
  `ToStdString(wxConvUTF8)`). `wxString` end-to-end.
- Remove toml11 fetch from CMake and `#include` from `pch.hpp`.
- Convert every `.toml` file under `resources/IDE/v2/` to `.ini`.

## Non-goals

- Do not restructure call sites.
- Do not redesign the settings dialog.
- Do not preserve bit-for-bit round-trip (comments / ordering may shift).

---

## Value redesign (API unchanged, new backing model)

### In-memory model

`Value` **is** the node. Owns its subtree. Move-only (no copy). Callers
always capture by reference so accidental clones are impossible.

```cpp
class Value final {
public:
    using Group = std::vector<std::pair<wxString, std::unique_ptr<Value>>>;
    //                       ^-- vector preserves insertion order
    //                       ^-- unique_ptr breaks recursive type

    Value() = default;                              // invalid (monostate)
    Value(const Value&) = delete;                   // no copy
    Value& operator=(const Value&) = delete;
    Value(Value&&) noexcept = default;              // move OK
    Value& operator=(Value&&) noexcept = default;

    [[nodiscard]] explicit operator bool() const noexcept;

    // Navigation — returns references into owned tree. operator[] may
    // create missing groups; at() returns a shared sentinel invalid Value
    // on miss (never dangling).
    [[nodiscard]] auto at(const wxString& path) const -> const Value&;
    [[nodiscard]] auto operator[](const wxString& path) -> Value&;

    // Leaf typed reads
    template<typename T> [[nodiscard]] auto as() const -> std::optional<T>;
    template<typename T> [[nodiscard]] auto value_or(T def) const -> T;
    template<typename P, typename T>
    [[nodiscard]] auto get_or(const P& path, T def) const -> T;

    // Arrays = leaf string split on ','
    [[nodiscard]] auto asArray() const -> std::vector<wxString>;

    // Ordered group iteration
    [[nodiscard]] auto entries() const -> const Group&;

    // Write — replaces this node's variant contents with a leaf
    template<typename T> auto operator=(T v) -> Value&;

private:
    std::variant<std::monostate, wxString, Group> m_data;
};
```

### Ownership + reference rules

- `ConfigManager` owns the root `Value` per category.
- All non-root `Value`s live inside their parent's `Group` vector,
  accessed via `unique_ptr<Value>`.
- Every accessor hands back a reference — `Value&` for mutable path,
  `const Value&` for const path.
- Callers **must** capture with `auto&` / `const auto&`. Attempting
  `auto x = cfg.at(...)` fails to compile (deleted copy). Compiler
  enforces reference semantics for us.

### Invalid-cursor semantics without cursor indirection

Read paths that miss need to return something. Options tried:

- Return `Value*` — leaks null pointers into caller code.
- Throw — changes API, forces try/catch everywhere.
- **Static `const Value` invalid singleton** — `at()` returns reference
  to a global `monostate` Value on miss. Fully constant; callers check
  via `operator bool` or just call `value_or(def)` which falls through.

Write path (`operator[]`) never misses — creates `Group` nodes on the
way, returns reference to newly inserted child.

### Key properties

- **Ordered**: `Group` is a vector of pairs — insertion order preserved
  on serialization.
- **Lookup**: linear scan per path segment. Config sections are small
  (< 50 entries typical); acceptable. Upgrade to sorted map or similar
  later if measured hot.
- **One type, one concept**: no separate `Node` — `Value` is the tree
  node. Simpler mental model.
- **Allocations**: one `unique_ptr<Value>` per child. ~100–500 total at
  startup; fine (confirmed not worth pmr).

### Read semantics

- `as<wxString>()` — returns leaf variant value.
- `as<int>()` / `as<bool>()` / `as<double>()` — parse leaf wxString.
  Uses `wxString::ToLong` / `CmpNoCase("true")` / `ToDouble`. Failure →
  nullopt; `value_or` falls through to default.
- `asArray()` — split leaf on `,`, trim whitespace per item. Empty leaf
  → empty vector.

### Write semantics

- `operator=(T)` — replaces the node's `m_data` with a leaf. `T` is
  bool/int/int64/double/wxString/std::string/const char*. Number/bool
  formatted as `"1"/"0"` or `"%d"` — consistent with wxFileConfig.
- `operator[](path)` navigates + auto-creates `Group` nodes along the
  way. If a leaf already exists at an intermediate segment, the write
  path needs to decide: overwrite to group (data loss) or fail. Choice:
  **fail silently** (invalid Value returned from operator[] when a
  conflict is detected); caller gets a no-op assignment. Alternative:
  assert. TBD during INI-02.

### Isolation from wxFileConfig

Value **does not** hold a `wxFileConfig*`. wxFileConfig is used only by
ConfigManager for file I/O (parse INI → Node tree on load; walk Node
tree → INI on save).

## ConfigManager redesign

- Entry struct: `Category, wxString path, Value root`.
  `Value` is move-only, `Entry` aggregate works with move, no unique_ptr
  needed for root.
- `load(Category)`:
  1. Open file with `wxFFileInputStream` (wide path on Windows).
  2. Construct `wxFileConfig(stream, wxConvUTF8)` — parsing only.
  3. Walk wxFileConfig groups + entries, build the `Value` tree in
     insertion order (use `wxFileConfig::GetFirstGroup`/`GetNextGroup` +
     `GetFirstEntry`/`GetNextEntry` recursively).
  4. Move the resulting `Value` into `entry.root`; drop the temporary
     `wxFileConfig`.
- `save(Category)`:
  1. Walk `entry.root` tree, emit INI directly to `wxFFileOutputStream`
     (`[group/subgroup]` headers + `key=value` lines; header per group
     with leaf children). Bypass wxFileConfig to avoid its reordering.
  2. Emit in vector order — preserves what we loaded.
- `get(Category)` — lazy load, return `Value&` (reference to
  `entry.root`).
- `enumerate()` — scan for `*.ini` (was `*.toml`).
- Filename resolution (`absolute`, `relative`) unchanged.
- File I/O switches to wx streams — side benefit: non-ASCII install paths
  on Windows now work without separate handling.

### Category accessor signatures

```cpp
[[nodiscard]] auto config()    -> Value&;
[[nodiscard]] auto locale()    -> Value&;
[[nodiscard]] auto theme()     -> Value&;
[[nodiscard]] auto shortcuts() -> Value&;
[[nodiscard]] auto keywords()  -> Value&;
[[nodiscard]] auto layout()    -> Value&;
```

All return by reference. `Context::tr` / UIManager / settings panels
already capture via `auto` today — must switch to `auto&` / `const auto&`
once `Value` becomes non-copyable. Compiler flags every missed site.

### Why our own emitter on save?

wxFileConfig re-emits from its internal hash-map — order not guaranteed
and unknown-key preservation is iffy on re-save. We want deterministic
output matching what's in the Node tree. Simple 20-line emitter:

```
[section/path]
key=value
; comment lines optional
```

### Parse-only use of wxFileConfig

wxFileConfig stays as our parser (handles quoting, comments, encoding).
Its in-memory representation is scanned once and discarded. This
isolates us from its save-side quirks.

## Array convention

- Comma-separated. Whitespace around commas trimmed on read.
- **Assumption**: no value in our config files contains a literal comma.
  Verified on current content (menu ids, shortcut strings, locale labels,
  file paths). If ever needed: escape with `\,` (handled in `asArray`).

## File conversions

Mechanical transformation per file:

- Top-level keys → before first `[section]` header.
- `[a.b.c]` → `[a/b/c]`.
- `key = [x, y, z]` → `key=x,y,z`.
- `key = true / false` → `key=1 / 0` (wxFileConfig’s bool format).
- Strings unquoted (INI values are raw text from `=` to end-of-line).
- Comments → `; comment` (INI convention; `#` also accepted by wxFileConfig).

## Encoding

- wxFileConfig built with `wxConvUTF8` reads/writes UTF-8. Verify via
  explicit ctor argument.
- Source files saved with BOM-less UTF-8.
- All `FromUTF8` / `utf8_str` / `ToStdString(wxConvUTF8)` calls in
  `Value.cpp`, `ConfigManager.cpp`, `UIManager.cpp`, and anywhere else
  touching config → delete.

## Risks / open questions

- **wxFileConfig ordering on save** — entries may reorder. Acceptable
  for our use (config files regenerated by app, not human-canonical).
- **Comment preservation** — wxFileConfig keeps unknown lines on load
  and re-emits on save; behaviour with edited files uncertain. Spot-check.
- **`ordered_type_config`** equivalent — toml11 preserved insertion
  order; wxFileConfig stores in `wxHashMap`. Re-saved files may look
  different. Not a correctness issue.
- **Numeric/bool parse semantics** — `cfg->Read("key", &intVar)` fails
  silently for malformed entries; `value_or` must fall back cleanly.
- **Layouts file has empty array** (`menu.recentFiles = []`) — translate
  to `recentFiles=` (empty value). `asArray` must treat empty string as
  empty vector.

## Rollback

Single feature branch. Revert with `git reset` if migration reveals a
blocker.

---

## TODO list

Execute roughly top-down. Build likely breaks between **INI-02-a** and
**INI-06-a** — this is expected during the rewrite.

### Preparation

- [ ] **INI-00-a** Audit all `.toml` content for commas inside values.
      Catalogue any hits; pick escape strategy if found.
- [ ] **INI-00-b** Confirm wxFileConfig ctor signature for explicit
      UTF-8: `wxFileConfig(wxInputStream&, const wxMBConv& = wxConvAuto{})`.

### File conversion (resources/IDE/v2)

- [ ] **INI-01-a** Convert `config_win.toml` → `config_win.ini`.
- [ ] **INI-01-b** Convert `layout.toml` → `layout.ini` (arrays as comma
      lists).
- [ ] **INI-01-c** Convert `shortcuts_win.toml` → `shortcuts_win.ini`.
- [ ] **INI-01-d** Convert `shortcuts_linux.toml` → `shortcuts_linux.ini`.
- [ ] **INI-01-e** Convert `shortcuts_macos.toml` → `shortcuts_macos.ini`.
- [ ] **INI-01-f** Convert `locales/en.toml` → `locales/en.ini`.
- [ ] **INI-01-g** Convert `locales/et.toml` → `locales/et.ini`.
- [ ] **INI-01-h** Convert `keywords.toml` → `keywords.ini` (if populated).
- [ ] **INI-01-i** Convert `themes/classic.toml` → `themes/classic.ini`.

### Value rewrite

- [ ] **INI-02-a** Rewrite `Value.hpp` — single type, owns its subtree,
      move-only, `std::variant<monostate, wxString, Group>` with
      `Group = std::vector<std::pair<wxString, std::unique_ptr<Value>>>`.
- [ ] **INI-02-b** Add deleted copy ctor / copy-assign; defaulted move.
- [ ] **INI-02-c** Add static `invalidValue()` sentinel — returned by
      const `at()` on miss.
- [ ] **INI-02-d** Implement `at(path)` — dot-split walk, returns
      `const Value&` (sentinel on miss).
- [ ] **INI-02-e** Implement `operator[](path)` — dot-split walk with
      auto-create of `Group` segments; returns `Value&`. Decide conflict
      policy (leaf-in-the-middle: reset node to Group and log, or
      assert).
- [ ] **INI-02-f** Implement typed `as<bool/int/int64/double/wxString>()`
      — parse leaf wxString per type, return `std::optional<T>`.
- [ ] **INI-02-g** Implement `value_or`, `get_or` — wrappers over
      `as<T>`.
- [ ] **INI-02-h** Implement `asArray` — split leaf wxString on `,`
      (trim each).
- [ ] **INI-02-i** Implement `entries()` — return `const Group&` for
      ordered iteration.
- [ ] **INI-02-j** Implement `operator=` for bool / int / int64 / double
      / wxString / std::string / const char\*. Replaces `m_data` with
      leaf. Numbers formatted via `wxString::Format`; bools as
      `"1"/"0"`.
- [ ] **INI-02-k** Audit existing `isTable/isArray/isString/isInt/
      isBool/isFloat` API — drop `isArray` (no native concept), keep
      `isTable` (variant holds `Group`) and leaf-probe `isString` /
      `isInt` / `isBool` / `isFloat`.
- [ ] **INI-02-l** Remove all `toml::` references from Value.
- [ ] **INI-02-m** Switch callers that used `auto cfg = ...` to
      `auto& cfg = ...` / `const auto& cfg = ...` — compiler finds them.

### ConfigManager rewrite

- [ ] **INI-03-a** Replace `ConfigValue` typedef with `Node`; Entry
      struct holds `std::unique_ptr<Node> root`.
- [ ] **INI-03-b** Implement INI-to-Node parser: use wxFileConfig to
      tokenise, walk its groups/entries recursively, populate `Node`
      tree in insertion order.
- [ ] **INI-03-c** Implement Node-to-INI emitter (own writer; ignore
      wxFileConfig::Save). Output: `[group/subgroup]` headers +
      `key=value` lines; one blank line between groups; UTF-8 bytes.
- [ ] **INI-03-d** Rewrite `load(Category)` — open via
      `wxFFileInputStream`, parse, store root.
- [ ] **INI-03-e** Rewrite `save(Category)` — open via
      `wxFFileOutputStream`, invoke emitter.
- [ ] **INI-03-f** Update `get(Category)` — return
      `Value{ entry.root.get() }`.
- [ ] **INI-03-g** Update `enumerate()` to match `*.ini`.
- [ ] **INI-03-h** Update `setCategoryPath` — route writes through Value
      (no toml-specific calls).

### Call-site audit (Value API stable, but clean up)

- [ ] **INI-04-a** Grep for `FromUTF8` / `utf8_str` / `utf8_string` /
      `ToStdString(wxConvUTF8)` and remove every one tied to config/locale.
- [ ] **INI-04-b** Drop `#include` of `toml::value` / toml11 helpers in
      `UIManager.cpp`, `Panel.cpp`, etc.
- [ ] **INI-04-c** Scan for any `toml::find`, `toml::find_or`,
      `ordered_type_config`, `toml::spec` — should be zero after INI-02 +
      INI-03.

### Dependency removal

- [ ] **INI-05-a** Remove `#include <toml.hpp>` (and any toml header)
      from `src/pch.hpp` / `src/lib/pch.hpp`.
- [ ] **INI-05-b** Remove toml11 `FetchContent_Declare` block from top
      CMakeLists.txt.
- [ ] **INI-05-c** Remove `toml11::toml11` link from `src/lib/CMakeLists.txt`.
- [ ] **INI-05-d** Reconfigure build from scratch to confirm toml11 not
      pulled.

### Verification

- [ ] **INI-06-a** Clean build green (debug + release).
- [ ] **INI-06-b** All 210 tests pass.
- [ ] **INI-06-c** Smoke test: launch fbide, render menus, open settings,
      change language to Estonian, restart — labels render correctly.
- [ ] **INI-06-d** Verify keywords editor round-trips after save.
- [ ] **INI-06-e** Verify window position / size persists across restart.

### Cleanup / docs

- [ ] **INI-07-a** Delete original `.toml` files under `resources/IDE/v2/`.
- [ ] **INI-07-b** Update `CLAUDE.md` if it mentions toml.
- [ ] **INI-07-c** Update memory: `reference_build.md` and any other
      auto-memory referencing toml11.

---

## Critical review of the plan

- **Atomic steps?** File conversions (INI-01-\*) are isolated — can be
  done before any code changes; just won't be picked up until INI-03-b
  lands. Rewrite steps (INI-02, INI-03) must land together — they share
  type boundaries (`Value` ↔ `ConfigManager`). Treat INI-02 + INI-03 as
  a single commit.
- **Build-breaking window** between INI-02-a and INI-06-a is unavoidable
  given the type change. Accept it; don't try a dual-backend scheme.
- **INI-00-a is real**: if any locale value contains a literal comma
  (e.g. future translations), INI-02-e must either escape or choose a
  different separator (`|` is safer against European languages). Run
  `grep -n ',' locales/*.toml` before conversion.
- **Ordering loss**: if human readability of saved INI files matters,
  add a post-save reformat step (read, group, rewrite with
  section-sorted keys). Deferred — not required for functionality.
- **No `isArray` in INI** — grep Value callers. If any depend on it,
  either make it always-true for leaves with commas, or drop the check
  and rely on `asArray().empty()`.
- **KeywordsPage runtime path** — editor re-reads keywords from toml on
  every `applyTheme()`. Same after INI swap, since Value API stable.
  Smoke test still needed.
- **Theme files**: `themes/classic.toml` is only read/parsed by
  `Theme.cpp`, which uses its own ad-hoc parser (separate from
  ConfigManager). Double-check what format Theme expects — it may
  already be INI-like, or may need its own conversion path. Verify
  before INI-01-i.

### Suggested commit boundaries

1. INI-00 audit + INI-01-\* conversions (safe pre-work, both file
   formats co-exist).
2. INI-02 + INI-03 + INI-04 together (type switch). Build-breaking, all
   fixed in one commit.
3. INI-05 dependency removal.
4. INI-06 verification (smoke test — no code change but tag).
5. INI-07 cleanup + docs.

Total estimated effort: 4–6 hours of focused work, most spent in file
conversions (INI-01-\*) and call-site cleanup (INI-04-\*).
