# snapctxmanager

Library and CLI tools for managing Tango attribute snapshot contexts in the
`snap` MySQL database.

---

## Contents

| Component | Type | Source | Description |
|-----------|------|--------|-------------|
| `libsnapctxmanager` | shared library | `snapctxmanager/` | CRUD for contexts, attributes and snapshots |
| `snapctx-man` | CLI | `snapctxmanager/snapctxman-cli/` | Manage contexts, list snapshots |
| `saverestore-save-cli` | CLI | `saverestore-save-cli/` | Read a context's attributes and save a snapshot |
| `saverestore-restore-cli` | CLI | `saverestore-restore-cli/` | Restore attribute values from a saved snapshot |

---

## Database connection

All tools share the same configuration file.  Each line has the form `key = value`:

```
dbhost = myserver.lab.local
dbname = snap
dbuser = snapuser
dbpass = secret
# dbport = 3306   (optional, default 3306)
```

### First-time setup

```sh
snapctx-man -i
```

Prompts for host, user, password and database name (password echo disabled) and
writes the file to `$HOME/.config/snapctxman-cli/snapctxman-cli.conf`.

### Custom config file

Pass `-c /path/to/my.conf` to any tool to override the default path.

---

## Typical workflow

```sh
# 1. Create a context (once)
snapctx-man -f attrs.txt -n "FEL1 tuning" -a "J. Smith" -r "initial baseline"

# 2. Save a snapshot before a machine change
saverestore-save-cli "FEL1 tuning" -m "before run 42"

# 3. List saved snapshots
snapctx-man -S "FEL1 tuning"
#  snap_id | time                | comment
# ---------+---------------------+----------------
#       17 | 2026-04-11 14:30:00 | before run 42

# 4. Restore snapshot 17 after the change
saverestore-restore-cli 17
```

---

## libsnapctxmanager

### Dependencies

- `sqldb-elettra` — MySQL abstraction layer (MySqlConnection, Result, Row, …)
- `tango` — Tango DeviceProxy used by `TgUtils` to query attribute properties
- `meson` ≥ 0.55, a C++17 compiler

### Build and install

```sh
cd snapctxmanager
meson setup builddir --prefix=/usr/local
cd builddir
ninja
ninja install          # installs headers, .so and snapctxmanager.pc
```

Headers land in `${prefix}/include/snapctxmanager/`.  
A pkg-config file `snapctxmanager.pc` is installed; downstream CMake/meson projects use
`pkg_check_modules(SNAPCTXMANAGER REQUIRED snapctxmanager)` /
`dependency('snapctxmanager')`.

> **Before `ninja install`:** point other projects at the build tree:
> ```sh
> export PKG_CONFIG_PATH=/path/to/snapctxmanager/builddir/meson-private
> ```

### Public API

```cpp
#include <snapctxmanager.h>   // SnapCtxManager
#include <snapdbschema.h>     // Context, Ast, Snapshot,
                              // SnapSaveRecord, SnapLoadRecord,
                              // snap_type_from_ast()
```

**Connecting**

```cpp
SnapCtxManager scm;
bool ok = scm.connect(SnapCtxManager::SNAPDBMYSQL,
                      host, dbname, user, password);
if(!ok) fprintf(stderr, "error: %s\n", scm.error().c_str());
```

**Context operations**

```cpp
// Create a context
Context ctx("my_ctx", "author", "reason", "description");
std::vector<std::string> srcs = { "a/b/c/attr1", "a/b/c/attr2" };
int rows = scm.register_context(ctx, srcs);          // > 0 on success

// Fetch context + attribute list by name or numeric id
Context c;
std::vector<Ast> atts;
bool found = scm.get_context("my_ctx", c, atts);

// Search by substring
std::vector<Context> results;
scm.search("FEL", results);

// All contexts
std::vector<Context> all = scm.ctxlist();

// Modify membership
scm.add_to_ctx("my_ctx", { "a/b/c/attr3" });
scm.remove_from_ctx("my_ctx", { "a/b/c/attr1" });

// Delete
scm.remove_ctx("my_ctx");
```

**Snapshot operations**

```cpp
// List snapshots for a context (newest first)
std::vector<Snapshot> snaps;
scm.snap_list(c.id, snaps);
// snaps[i]: .id_snap, .id_context, .time, .comment

// Save a snapshot
std::vector<SnapSaveRecord> records;
for(const Ast& a : atts) {
    SnapSaveRecord r;
    r.id_att    = a.id;
    r.snap_type = snap_type_from_ast(a.data_type, a.data_format, a.writable);
    r.value     = "3.14";      // "NULL" or "" = read error (row skipped)
    r.setpoint  = "3.14";      // only for *2 (READ_WRITE) types
    r.dim_x     = 0;           // only for spectra
    records.push_back(r);
}
int snap_id = scm.snap_save(c.id, "my comment", records);  // > 0 on success

// Load all values from a snapshot
std::vector<SnapLoadRecord> loaded;
scm.snap_load(snap_id, loaded);
// loaded[i]: .full_name, .device, .value, .setpoint, .dim_x, .snap_type,
//            .data_type, .data_format, .writable

// Query stored values for specific attributes (cross-context, all snapshots)
std::vector<AttSnapRecord> recs;
scm.snap_query_by_atts({"a/b/c/attr1", "a/b/c/attr2"}, recs);
// recs[i]: .full_name, .ctx_id, .ctx_name, .snap_id, .snap_time,
//          .snap_comment, .value, .setpoint, .dim_x, .snap_type,
//          .data_type, .data_format, .writable

// Remove attributes from context — keep snapshot data (default)
scm.remove_from_ctx("my_ctx", {"a/b/c/attr1"});
// scm.warning() → "Attribute(s) removed. Snapshot data left in place. Use --purge..."

// Remove attributes and purge all snapshot values for this context
scm.remove_from_ctx("my_ctx", {"a/b/c/attr1"}, /*purge_data=*/true);
// scm.warning() → "Snapshot data deleted. Omit --purge to keep data..."
```

**Error handling**

```cpp
if(scm.error().length() > 0)
    fprintf(stderr, "error: %s\n", scm.error().c_str());
if(scm.warning().length() > 0)
    fprintf(stderr, "warning: %s\n", scm.warning().c_str());
```

### SnapType reference

`snap_type_from_ast(data_type, data_format, writable)` maps raw Tango integer
codes (stored in `ast`) to the value table used for that attribute:

| SnapType    | Table            | Tango format / write mode |
|-------------|------------------|---------------------------|
| `stScNum1`  | `t_sc_num_1val`  | SCALAR numeric, READ |
| `stScNum2`  | `t_sc_num_2val`  | SCALAR numeric, READ_WRITE |
| `stScStr1`  | `t_sc_str_1val`  | SCALAR string, READ |
| `stScStr2`  | `t_sc_str_2val`  | SCALAR string, READ_WRITE |
| `stSp1`     | `t_sp_1val`      | SPECTRUM, READ |
| `stSp2`     | `t_sp_2val`      | SPECTRUM, READ_WRITE |
| `stIm1/2`   | *(not inserted)* | IMAGE — reserved |

---

## snapctx-man

Context and snapshot management CLI.

### Build

```sh
cd snapctxmanager/snapctxman-cli
meson setup builddir --prefix=/usr/local
cd builddir && ninja && ninja install
```

Meson options:

| Option | Default | Description |
|--------|---------|-------------|
| `conf_file` | `snapctxman-cli.conf` | Config file name |
| `conf_dir`  | `$HOME/.config/snapctxman-cli` | Config file directory |

### Options

| Option | Argument | Description |
|--------|----------|-------------|
| `-i`   | —        | Interactive first-time DB configuration |
| `-c`   | `file`   | Custom database config file |
| `-n`   | `name`   | Show context by exact name |
| `-s`   | `keyword`| Search contexts by substring (`%` = all) |
| `-l`   | —        | List all contexts |
| `-f`   | `file\|list` | Attribute source file (one per line) or comma-separated list |
| `-a`   | `author` | Author (required when creating a context) |
| `-r`   | `reason` | Reason (required when creating a context) |
| `-d`   | `desc`   | Description |
| `-A`   | —        | Add `-f` sources to existing context `-n` |
| `-D`   | —        | Delete context `-n`, or delete `-f` sources from it |
| `-P`   | —        | Purge snapshot data when removing attributes (use with `-D -f`) |
| `-R`   | `list`   | Comma-separated new attribute names (rename; pair with `-f` for old names) |
| `-S`   | `name\|id` | List snapshots for the named or numeric-id context |
| `-q`   | `list`   | Query stored values for comma-separated attribute names (cross-context) |
| `-h`   | —        | Show full help |

### Examples

```sh
snapctx-man -i                                         # first-time setup
snapctx-man -n "FEL1 tuning"                           # show context
snapctx-man -s "FEL"                                   # search
snapctx-man -s "%"                                     # list all
snapctx-man -l                                         # list all (compact)
snapctx-man -S "FEL1 tuning"                           # list snapshots
snapctx-man -S 42                                      # list snapshots by id

# Create from file (one attribute per line)
snapctx-man -f attrs.txt -n "My Context" -a "J. Smith" -r "reason"

# Create from comma-separated list (trailing comma required for a single attr)
snapctx-man -f "a/b/c/attr1,a/b/c/attr2," -n "My Context" -a "J. Smith" -r "reason"

# Modify
snapctx-man -A -f new.txt  -n "My Context"             # add attributes
snapctx-man -D -f old.txt  -n "My Context"             # remove, keep snapshot data
snapctx-man -D -P -f old.txt -n "My Context"           # remove + purge snapshot data
snapctx-man -D             -n "My Context"             # delete context

# Rename (old names via -f, new names via -R, same order)
snapctx-man -f "a/b/c/old1,a/b/c/old2," -R "a/b/c/new1,a/b/c/new2"

# Query stored values for specific attributes (across all contexts)
snapctx-man -q "a/b/c/attr1,a/b/c/attr2"
```

---

## saverestore-save-cli

Reads all Tango attributes in a snap context once and saves a snapshot to the DB.  
Uses the `cumbia-multiread-plugin` for concurrent or sequential reads.

### Build

```sh
cd saverestore-save-cli
# only needed before snapctxmanager is installed:
export PKG_CONFIG_PATH=/path/to/snapctxmanager/builddir/meson-private
mkdir build && cd build
cmake ..
make
```

### Options

```
saverestore-save-cli <context_name_or_id> [options]
```

| Option | Description |
|--------|-------------|
| `context_name_or_id` | Context name (exact) or numeric id |
| `-s`, `--sequential` | Read attributes one by one (default: concurrent) |
| `-m`, `--message` `"text"` | Comment stored in the `snapshot` row |
| `-o`, `--output` `file` | Tab-separated log: `attribute  value  setpoint  data_type  data_format  OK\|NOT_OK  error` |
| `-c`, `--conf` `file` | DB config file (default: `~/.config/snapctxman-cli/snapctxman-cli.conf`) |
| `-t`, `--timeout` `N` | Seconds to wait for Tango reads (default: 10) |

### Examples

```sh
saverestore-save-cli "FEL1 tuning" -m "before run 42"
saverestore-save-cli 5 -s -m "sequential baseline" -o save.log
snapctx-man -S "FEL1 tuning"   # check the resulting snapshot id
```

### Behaviour

- **Concurrent mode** (default): subscribes all sources at once; saves when the
  first result for every source has arrived.
- **Sequential mode** (`-s`): reads attributes one by one via `SequentialManual`;
  saves on cycle completion.
- Attributes that time out receive a `NULL` value in the DB (same as the Qt app).
- Spectrum values are stored comma-separated.

---

## saverestore-restore-cli

Loads a snapshot from the DB and writes each attribute value back to its
Tango device using `QuWriter`.

### Build

```sh
cd saverestore-restore-cli
export PKG_CONFIG_PATH=/path/to/snapctxmanager/builddir/meson-private  # if not installed
mkdir build && cd build
cmake ..
make
```

### Options

```
saverestore-restore-cli <snap_id> [options]
```

| Option | Description |
|--------|-------------|
| `snap_id` | Numeric snapshot id (from `snapctx-man -S`) |
| `-o`, `--output` `file` | Tab-separated log: `attribute  write_value  data_type  data_format  OK\|NOT_OK  error` |
| `-c`, `--conf` `file` | DB config file (default: `~/.config/snapctxman-cli/snapctxman-cli.conf`) |

### Examples

```sh
saverestore-restore-cli 17
saverestore-restore-cli 17 -o restore.log
```

### Behaviour

- **READ-only** attributes, **IMAGE** format, and **NULL set-points** are skipped
  (printed as `SKIP` with the reason).
- For **READ_WRITE** attributes the stored **set-point** (write value) is restored.
- For **WRITE-only** attributes the stored **value** column is used.
- One `QuWriter` per attribute is created; the write is queued and fires
  automatically on device connection.
- The process exits with code `0` (all ok) or `1` (at least one write error)
  after every write result has been received.

---

## Database schema reference

Tables in the `snap` database:

| Table | Description |
|-------|-------------|
| `context` | One row per context (`id_context`, `name`, `author`, `reason`, `description`, `time`) |
| `ast` | One row per Tango attribute (`ID`, `full_name`, `device`, `data_type`, `data_format`, `writable`, `facility`, …) |
| `list` | Many-to-many join: `id_context` ↔ `id_att` |
| `snapshot` | One row per saved snapshot (`id_snap`, `id_context`, `time`, `snap_comment`) |
| `t_sc_num_1val` | Scalar numeric, READ: `(id_snap, id_att, value)` |
| `t_sc_num_2val` | Scalar numeric, READ_WRITE: `(id_snap, id_att, value, setpoint)` |
| `t_sc_str_1val` | Scalar string, READ: `(id_snap, id_att, value)` |
| `t_sc_str_2val` | Scalar string, READ_WRITE: `(id_snap, id_att, value, setpoint)` |
| `t_sp_1val` | Spectrum, READ: `(id_snap, id_att, dim_x, value)` |
| `t_sp_2val` | Spectrum, READ_WRITE: `(id_snap, id_att, dim_x, value, setpoint)` |

Attributes that could not be read are **not inserted** (row omitted = NULL behaviour),
matching the Qt saverestore application.

---

## Contact

giacomo.strangolino@elettra.eu  
Source: https://github.com/ELETTRA-SincrotroneTrieste/snapctxmanager
