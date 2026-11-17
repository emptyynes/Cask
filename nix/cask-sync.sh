# cask-sync: brings the Cask folder (runners, components, prefixes) in line with the NixOS configuration.
#
# Started by the `cask-sync` systemd service of the module (nix/declarative.nix) as the user whose
# Cask folder it is. Usage: cask-sync MANIFEST.json   (environment: CASK_SYNC_ROOT, CASK_SYNC_XVFB)
#
# Everything here is idempotent: what already matches the configuration is not touched, so a repeated
# run does not download, copy or re-run anything. The layout and the cask.json format are Cask's own
# (see Config, Prefix and PrefixManager in the sources of `main`); the sources themselves are not changed.
#
# Terms: runner = a Wine build in data/runners/<name>; component = DXVK, D7VK or VKD3D-Proton in
# data/<type>/<version>; prefix = prefixes/<name> with cask.json; verb = a winetricks verb.
#
# This file is the body of a writeShellApplication (nix/declarative.nix): the shebang and
# `set -o nounset -o pipefail` are added there. Errors are handled explicitly (no `errexit`),
# so that a failure in one prefix does not stop the others.

MANIFEST=${1:?usage: cask-sync MANIFEST.json}
ROOT=${CASK_SYNC_ROOT:?CASK_SYNC_ROOT is not set}
PREFIXES="$ROOT/prefixes"
FAILED=0

log() { printf 'cask-sync: %s\n' "$*"; }
warn() { printf 'cask-sync: warning: %s\n' "$*" >&2; }
err() { printf 'cask-sync: error: %s\n' "$*" >&2; }

# Winetricks verbs and GUI installers need an X server; the service has none, so it brings a virtual one.
if [ "${CASK_SYNC_XVFB:-0}" = 1 ] && [ -z "${DISPLAY:-}" ]; then
  exec xvfb-run -a "$0" "$@"
fi

shopt -s nullglob dotglob

mkdir -p "$PREFIXES" "$ROOT/data/runners" "$ROOT/data/winetricks/cache" \
  "$ROOT/data/dxvk" "$ROOT/data/d7vk" "$ROOT/data/vkd3d-proton"

# One run at a time (the service and a manual `systemctl restart` must not overlap).
exec 9>"$ROOT/.cask-sync.lock"
flock 9

# ---------------------------------------------------------------------------------------------
# cask.json
# ---------------------------------------------------------------------------------------------

# cask_json_update FILE FILTER [jq args...]: atomic rewrite, formatted like Qt does (4 spaces, sorted keys).
cask_json_update() {
  local file=$1 filter=$2 tmp
  shift 2
  tmp=$(mktemp "$file.XXXXXX") || return 1
  if jq --indent 4 -S "$@" "$filter" "$file" >"$tmp"; then
    chmod 644 "$tmp"
    mv -f "$tmp" "$file"
  else
    rm -f "$tmp"
    return 1
  fi
}

# cask_json_create DIR RUNNER ARCH: the same content that Prefix::save() writes for a new prefix.
cask_json_create() {
  local dir=$1 runner=$2 arch=$3 tmp
  tmp=$(mktemp "$dir/cask.json.XXXXXX") || return 1
  if jq -n --indent 4 -S --arg runner "$runner" --arg arch "$arch" --arg created "$(date +%Y-%m-%dT%H:%M:%S)" \
    '{runner: $runner, arch: $arch, created: $created, components: {},
      settings: {locale: "", gamemode: false, mangohud: false, env: {}}}' >"$tmp"; then
    chmod 644 "$tmp"
    mv -f "$tmp" "$dir/cask.json"
  else
    rm -f "$tmp"
    return 1
  fi
}

# ---------------------------------------------------------------------------------------------
# runners and components: symbolic links to the store
# ---------------------------------------------------------------------------------------------

# link_managed DIR NAME TARGET: DIR/NAME -> TARGET (a path in the Nix store).
# Only links that this script made (they point into /nix/store/*-cask-*) are ever replaced.
link_managed() {
  local dir=$1 name=$2 target=$3 link cur
  link="$dir/$name"
  if [ -L "$link" ]; then
    cur=$(readlink "$link")
    [ "$cur" = "$target" ] && return 0
    case "$cur" in
      /nix/store/*-cask-*)
        ln -sfn "$target" "$link" && log "updated $link"
        return 0
        ;;
      *)
        warn "$link is a symbolic link that Nix did not create; left as it is"
        return 0
        ;;
    esac
  elif [ -e "$link" ]; then
    warn "$link already exists and was not created by Nix; left as it is"
    return 0
  fi
  ln -s "$target" "$link" && log "linked $link"
}

# remove_stale DIR KIND NAME...: removes our links in DIR that are no longer declared (NAME... are the declared ones).
remove_stale() {
  local dir=$1 kind=$2 entry name cur keep declared
  shift 2
  for entry in "$dir"/*; do
    [ -L "$entry" ] || continue
    cur=$(readlink "$entry")
    case "$cur" in
      /nix/store/*-cask-"$kind"-*) ;;
      *) continue ;;
    esac
    name=${entry##*/}
    keep=0
    for declared in "$@"; do
      [ "$declared" = "$name" ] && keep=1
    done
    if [ "$keep" -eq 0 ]; then
      rm -f "$entry" && log "removed $entry (not declared any more)"
    fi
  done
  return 0
}

# ---------------------------------------------------------------------------------------------
# Wine
# ---------------------------------------------------------------------------------------------

# resolve_runner NAME: sets RUNNER_BIN (put in front of PATH; empty for the system Wine), WINE, WINESERVER.
# Same rules as RunnerManager: data/runners/<name>/bin/wine, or wine64.
resolve_runner() {
  local name=$1 bin
  RUNNER_BIN=""
  if [ "$name" = system ]; then
    WINE=$(command -v wine) || {
      err "the system Wine was not found in PATH (set programs.cask.wine)"
      return 1
    }
    WINESERVER=$(command -v wineserver || echo wineserver)
    return 0
  fi
  bin="$ROOT/data/runners/$name/bin"
  WINE="$bin/wine"
  [ -x "$WINE" ] || WINE="$bin/wine64"
  if [ ! -x "$WINE" ]; then
    err "runner '$name': no executable wine in $bin"
    return 1
  fi
  WINESERVER="$bin/wineserver"
  RUNNER_BIN=$bin
}

# prefix_env PREFIXDIR ARCH: fills ENVV with what PrefixManager::makeEnv(forLaunch = false) sets.
prefix_env() {
  local p=$1 arch=$2 locale=""
  mkdir -p "$p/cache/dxvk_shader" "$p/cache/mesa_shader" "$p/cache/vkd3d_shader"
  ENVV=(
    "WINEPREFIX=$p"
    "WINEARCH=$arch"
    "WINEDEBUG=${WINEDEBUG:--all}"
    "DXVK_STATE_CACHE_PATH=$p/cache/dxvk_shader"
    "DXVK_SHADER_CACHE_PATH=$p/cache/dxvk_shader"
    "MESA_SHADER_CACHE_DIR=$p/cache/mesa_shader"
    "VKD3D_SHADER_CACHE_PATH=$p/cache/vkd3d_shader"
  )
  [ -n "$RUNNER_BIN" ] && ENVV+=("PATH=$RUNNER_BIN:$PATH")
  [ -f "$p/cask.json" ] && locale=$(jq -r '.settings.locale // ""' "$p/cask.json")
  [ -n "$locale" ] && ENVV+=("LANG=$locale" "LC_ALL=$locale")
  return 0
}

# stop_prefix PREFIXDIR RUNNER ARCH: wineserver -k of the Wine that the prefix has been using.
stop_prefix() {
  local p=$1 runner=$2 arch=$3
  resolve_runner "$runner" 2>/dev/null || return 0
  prefix_env "$p" "$arch"
  env "${ENVV[@]}" "$WINESERVER" -k 2>/dev/null || true
}

# unlink_user_dirs PREFIXDIR: PrefixManager::unlinkUserDirs. Desktop, Documents... must not be links
# to the home folder: links that point outside the prefix become empty folders.
unlink_user_dirs() {
  local p=$1 users="$1/drive_c/users" root canon profile entry target
  [ -d "$users" ] || return 0
  root=$(realpath -m -s "$p")
  canon=$(realpath -m "$p")
  for profile in "$users"/*; do
    [ -L "$profile" ] && continue
    [ -d "$profile" ] || continue
    for entry in "$profile"/*; do
      [ -L "$entry" ] || continue
      target=$(readlink "$entry")
      case "$target" in /*) ;; *) target="$profile/$target" ;; esac
      target=$(realpath -m -s "$target")
      case "$target" in "$root"/* | "$canon"/*) continue ;; esac
      rm -f "$entry" && mkdir -p "$entry"
    done
  done
  return 0
}

# init_prefix PREFIXDIR ARCH: `wineboot --init` without the Mono/Gecko dialogs, like PrefixManager::create.
# Needs resolve_runner to have been called.
init_prefix() {
  local p=$1 arch=$2
  prefix_env "$p" "$arch"
  if ! env "${ENVV[@]}" WINEDLLOVERRIDES="mscoree,mshtml=d" "$WINE" wineboot --init; then
    err "wineboot --init failed in $p"
    return 1
  fi
  env "${ENVV[@]}" "$WINESERVER" -w
  unlink_user_dirs "$p"
}

# ---------------------------------------------------------------------------------------------
# components (DXVK, D7VK, VKD3D-Proton): PrefixManager::applyComponent / restoreFiles / importOverrides
# ---------------------------------------------------------------------------------------------

# override_names FILE...: the DLL names without extension, lower case, unique, sorted (overrideNames()).
override_names() {
  local f b
  for f in "$@"; do
    b=${f##*/}
    b=${b%.*}
    printf '%s\n' "${b,,}"
  done | sort -u
}

# regedit_import PREFIXDIR ARCH LINES: imports LINES (newline-separated, `"name"="value"` or `"name"=-`) into
# HKCU\Software\Wine\DllOverrides with regedit. Needs resolve_runner to have been called.
regedit_import() {
  local p=$1 arch=$2 lines=$3 reg="$1/cask-overrides.reg" line rc
  {
    printf '%s\r\n\r\n' 'Windows Registry Editor Version 5.00'
    printf '%s\r\n' '[HKEY_CURRENT_USER\Software\Wine\DllOverrides]'
    while IFS= read -r line; do
      if [ -n "$line" ]; then printf '%s\r\n' "$line"; fi
    done <<<"$lines"
  } >"$reg" || return 1
  prefix_env "$p" "$arch"
  # The file is passed by its relative name from the prefix folder: more reliable than a unix path in regedit.
  (cd "$p" && env "${ENVV[@]}" "$WINE" regedit /S cask-overrides.reg)
  rc=$?
  env "${ENVV[@]}" "$WINESERVER" -w
  rm -f "$reg"
  return $rc
}

# import_overrides PREFIXDIR ARCH SET_NAMES REMOVE_NAMES: the native,builtin overrides of a component
# (SET_NAMES and REMOVE_NAMES are newline-separated lists of DLL names).
import_overrides() {
  local p=$1 arch=$2 set_names=$3 remove_names=$4 lines="" n
  while IFS= read -r n; do
    if [ -n "$n" ]; then lines+="\"$n\"=\"native,builtin\""$'\n'; fi
  done <<<"$set_names"
  while IFS= read -r n; do
    if [ -n "$n" ]; then lines+="\"$n\"=-"$'\n'; fi
  done <<<"$remove_names"
  [ -z "$lines" ] && return 0
  regedit_import "$p" "$arch" "$lines"
}

# current_override PREFIXDIR NAME: the value of NAME in HKCU\Software\Wine\DllOverrides as Wine has saved it
# in user.reg (the name is not case sensitive); fails if there is none.
current_override() {
  awk -v want="$2" '
    BEGIN { w = tolower(want); found = 0 }
    /^\[/ { insec = (tolower($0) ~ /^\[software\\\\wine\\\\dlloverrides\]/); next }
    insec && /^"/ {
      n = index($0, "\"=\"")
      if (n == 0) next
      name = substr($0, 2, n - 2)
      val = substr($0, n + 3)
      sub(/"[ \t]*$/, "", val)
      if (tolower(name) == w) { print val; found = 1; exit }
    }
    END { exit found ? 0 : 1 }
  ' "$1/user.reg"
}

# restore_files PREFIXDIR TYPE: rolls back the files of the component that is applied now. Sets
# STALE_OVERRIDES (newline-separated): override names that no other component needs any more.
restore_files() {
  local p=$1 type=$2 win="$1/drive_c/windows" rel names used
  local -a files=()
  STALE_OVERRIDES=""
  mapfile -t files < <(jq -r --arg t "$type" '.components[$t].files // [] | .[]' "$p/cask.json")
  [ "${#files[@]}" -eq 0 ] && return 0
  names=$(override_names "${files[@]}")
  for rel in "${files[@]}"; do
    rm -f "$win/$rel"
    [ -e "$win/$rel.cask-bak" ] && mv -f "$win/$rel.cask-bak" "$win/$rel"
  done
  cask_json_update "$p/cask.json" 'del(.components[$t])' --arg t "$type" || return 1
  mapfile -t files < <(jq -r '.components[].files[]?' "$p/cask.json")
  used=""
  [ "${#files[@]}" -gt 0 ] && used=$(override_names "${files[@]}")
  STALE_OVERRIDES=$(comm -23 <(printf '%s\n' "$names") <(printf '%s\n' "$used"))
}

# component_intact PREFIXDIR TYPE VERSION: this version is recorded in cask.json and all its files are there.
component_intact() {
  local p=$1 type=$2 ver=$3 rel
  [ "$(jq -r --arg t "$type" '.components[$t].version // ""' "$p/cask.json")" = "$ver" ] || return 1
  while IFS= read -r rel; do
    [ -e "$p/drive_c/windows/$rel" ] || return 1
  done < <(jq -r --arg t "$type" '.components[$t].files // [] | .[]' "$p/cask.json")
}

# plan_add SRCROOT TARGET SUBDIR...: adds to the copy plan (PLAN_SRC, PLAN_REL) the DLLs of the first of
# the SUBDIRs that exists in SRCROOT; they go to drive_c/windows/TARGET.
plan_add() {
  local srcroot=$1 target=$2 dir dll
  shift 2
  for dir in "$@"; do
    [ -d "$srcroot/$dir" ] || continue
    for dll in "$srcroot/$dir"/*.dll; do
      [ -f "$dll" ] || continue
      PLAN_SRC+=("$dll")
      PLAN_REL+=("$target/${dll##*/}")
    done
    break
  done
}

# apply_component PREFIXDIR TYPE VERSION: copies the DLLs from data/<type>/<version> into the prefix
# (the originals are kept once as *.cask-bak), sets the overrides and records the files in cask.json.
# Needs resolve_runner to have been called.
apply_component() {
  local p=$1 type=$2 ver=$3 src="$ROOT/data/$2/$3" win="$1/drive_c/windows"
  local arch rel dst new_set new_remove i
  local -a d32 done_rel=()
  PLAN_SRC=()
  PLAN_REL=()

  if [ ! -d "$src" ]; then
    err "$type $ver: folder not found: $src"
    return 1
  fi
  if [ ! -d "$win/system32" ]; then
    err "$type $ver: the prefix is not initialised yet (no drive_c/windows/system32)"
    return 1
  fi
  arch=$(jq -r '.arch // "win64"' "$p/cask.json")
  if [ "$type" = vkd3d-proton ]; then d32=(x86 x32); else d32=(x32 x86); fi

  # Copy plan, like PrefixManager: 64-bit DLLs to system32 and 32-bit ones to syswow64 (win64 prefix),
  # only the 32-bit ones to system32 (win32 prefix).
  if [ "$arch" = win64 ]; then
    plan_add "$src" system32 x64
    plan_add "$src" syswow64 "${d32[@]}"
  else
    plan_add "$src" system32 "${d32[@]}"
  fi
  if [ "${#PLAN_SRC[@]}" -eq 0 ]; then
    err "$type $ver: no DLLs in $src (subfolders x64 and x32/x86 are expected)"
    return 1
  fi

  # The old version of this component is rolled back first.
  restore_files "$p" "$type" || return 1

  for i in "${!PLAN_SRC[@]}"; do
    rel=${PLAN_REL[$i]}
    dst="$win/$rel"
    if [ -e "$dst" ] && [ ! -e "$dst.cask-bak" ]; then
      mv -f "$dst" "$dst.cask-bak"
    fi
    rm -f "$dst"
    if ! cp -- "${PLAN_SRC[$i]}" "$dst"; then
      err "$type $ver: could not copy ${PLAN_SRC[$i]}"
      done_rel+=("$rel")
      for rel in "${done_rel[@]}"; do
        rm -f "$win/$rel"
        [ -e "$win/$rel.cask-bak" ] && mv -f "$win/$rel.cask-bak" "$win/$rel"
      done
      return 1
    fi
    chmod u+w "$dst"
    done_rel+=("$rel")
  done

  new_set=$(override_names "${done_rel[@]}")
  new_remove=$(comm -23 <(printf '%s\n' "$STALE_OVERRIDES" | sort -u) <(printf '%s\n' "$new_set"))
  import_overrides "$p" "$arch" "$new_set" "$new_remove" || {
    err "$type $ver: could not set the DLL overrides (regedit failed)"
    return 1
  }

  # Recorded last: if anything above failed, the next run tries again. The files now belong to this
  # component, so they are removed from the lists of the others (the last one applied wins).
  cask_json_update "$p/cask.json" \
    '.components |= with_entries(.value.files -= $files) | .components[$t] = {version: $v, files: $files}' \
    --arg t "$type" --arg v "$ver" --argjson files "$(printf '%s\n' "${done_rel[@]}" | jq -R . | jq -s .)" || return 1
  log "$type $ver applied to $(basename "$p")"
}

# ---------------------------------------------------------------------------------------------
# winetricks
# ---------------------------------------------------------------------------------------------

# verb_installed PREFIXDIR VERB: winetricks appends every successfully run verb to winetricks.log
# (this is also all that `winetricks list-installed` shows).
verb_installed() {
  grep -qxF -- "$2" "$1/winetricks.log" 2>/dev/null
}

# run_verb PREFIXDIR ARCH VERB: the same script as Cask uses: <data>/winetricks/winetricks if it exists, else from PATH.
# Needs resolve_runner to have been called.
run_verb() {
  local p=$1 arch=$2 verb=$3 local_script="$ROOT/data/winetricks/winetricks" rc
  local -a cmd=(winetricks)
  if [ -f "$local_script" ]; then
    if [ -x "$local_script" ]; then cmd=("$local_script"); else cmd=(sh "$local_script"); fi
  fi
  prefix_env "$p" "$arch"
  env "${ENVV[@]}" WINE="$WINE" WINESERVER="$WINESERVER" W_CACHE="$ROOT/data/winetricks/cache" \
    "${cmd[@]}" -q "$verb"
  rc=$?
  env "${ENVV[@]}" "$WINESERVER" -w
  return $rc
}

# ---------------------------------------------------------------------------------------------
# prefixes
# ---------------------------------------------------------------------------------------------

# prefix_problem PREFIXDIR: integrity check of a prefix that has a valid cask.json. Prints what is wrong
# and returns 0 if something is; returns 1 if the prefix is fine, or if it was never initialised
# (no registry and no system32 at all: that is a new or interrupted creation, finished in place).
# The checks are structural only; what is installed in the prefix is the user's business.
prefix_problem() {
  local p=$1 arch f
  arch=$(jq -r '.arch' "$p/cask.json")
  if [ ! -e "$p/system.reg" ] && [ ! -d "$p/drive_c/windows/system32" ]; then
    return 1
  fi
  for f in system.reg user.reg userdef.reg; do
    if [ ! -s "$p/$f" ]; then
      echo "$f is missing or empty"
      return 0
    fi
    if [ "$(head -n 1 "$p/$f")" != "WINE REGISTRY Version 2" ]; then
      echo "$f is not a Wine registry file"
      return 0
    fi
  done
  if ! grep -qx "#arch=$arch" "$p/system.reg"; then
    echo "system.reg is not of the architecture $arch from cask.json"
    return 0
  fi
  for f in kernel32.dll ntdll.dll; do
    if [ ! -s "$p/drive_c/windows/system32/$f" ]; then
      echo "drive_c/windows/system32/$f is missing"
      return 0
    fi
  done
  if [ "$arch" = win64 ] && [ ! -d "$p/drive_c/windows/syswow64" ]; then
    echo "drive_c/windows/syswow64 is missing"
    return 0
  fi
  return 1
}

# set_aside NAME LABEL: stops the wineserver of the prefix and renames it to <name>.<label>-<date>
# (nothing is deleted). Used for a prefix whose declared architecture changed (it cannot be changed in place).
set_aside() {
  local name=$1 label=$2 p="$PREFIXES/$1" old arch backup
  if [ -f "$p/cask.json" ] && old=$(jq -r '.runner // "system"' "$p/cask.json" 2>/dev/null); then
    arch=$(jq -r '.arch // "win64"' "$p/cask.json" 2>/dev/null)
    stop_prefix "$p" "$old" "$arch"
  fi
  backup="$PREFIXES/$name.$label-$(date +%Y%m%d-%H%M%S)"
  while [ -e "$backup" ]; do backup="$backup-again"; done
  mv -T -- "$p" "$backup" || return 1
  warn "prefix '$name' was set aside as '${backup##*/}'"
}

# sync_settings NAME PREFIXDIR ARCH: the environment variables (settings.env of cask.json, what Cask gives to
# the programs it starts) and the DLL overrides (registry) of the configuration. Only what Nix has put there
# earlier is ever removed; what the user set himself stays. What Nix manages is remembered in
# cask-nix-state.json in the prefix, not in cask.json: Cask rewrites that with its own keys only.
# Needs resolve_runner to have been called.
sync_settings() {
  local name=$1 p=$2 arch=$3 state="$2/cask-nix-state.json"
  local want_env want_ov prev_env="[]" prev_ov="[]" drop_env new_state have cur n v lines=""
  want_env=$(jq -c --arg n "$name" '.prefixes[$n].env' "$MANIFEST")
  want_ov=$(jq -c --arg n "$name" '.prefixes[$n].dllOverrides' "$MANIFEST")
  if [ ! -f "$state" ] && [ "$want_env" = "{}" ] && [ "$want_ov" = "{}" ]; then
    return 0
  fi
  if [ -f "$state" ]; then
    prev_env=$(jq -c '.env // []' "$state" 2>/dev/null) || prev_env="[]"
    prev_ov=$(jq -c '.dllOverrides // []' "$state" 2>/dev/null) || prev_ov="[]"
  fi

  # Environment variables.
  drop_env=$(jq -nc --argjson prev "$prev_env" --argjson want "$want_env" '$prev - ($want | keys)')
  if ! jq -e --argjson want "$want_env" --argjson drop "$drop_env" '
      (.settings.env // {}) as $cur
      | ($want | to_entries | all(.value == $cur[.key]))
        and ($drop | all(. as $k | $cur | has($k) | not))' "$p/cask.json" >/dev/null; then
    cask_json_update "$p/cask.json" \
      '.settings.env = ((.settings.env // {}) | with_entries(select(.key as $k | $drop | index($k) | not))) + $want' \
      --argjson want "$want_env" --argjson drop "$drop_env" || return 1
    log "prefix '$name': environment variables updated"
  fi

  # DLL overrides: set the ones that differ, remove the ones that Nix set earlier and are not declared now.
  while IFS=$'\t' read -r n v; do
    [ -z "$n" ] && continue
    have=0
    if cur=$(current_override "$p" "$n"); then have=1; fi
    if [ "$have" -eq 0 ] || [ "$cur" != "$v" ]; then
      lines+="\"$n\"=\"$v\""$'\n'
    fi
  done < <(jq -r 'to_entries[] | [.key, .value] | @tsv' <<<"$want_ov")
  while IFS= read -r n; do
    [ -z "$n" ] && continue
    if current_override "$p" "$n" >/dev/null; then
      lines+="\"$n\"=-"$'\n'
    fi
  done < <(jq -nr --argjson prev "$prev_ov" --argjson want "$want_ov" '$prev - ($want | keys) | .[]')
  if [ -n "$lines" ]; then
    regedit_import "$p" "$arch" "$lines" || {
      err "prefix '$name': could not set the DLL overrides (regedit failed)"
      return 1
    }
    log "prefix '$name': DLL overrides updated"
  fi

  new_state=$(jq -nc --argjson env "$want_env" --argjson ov "$want_ov" '{env: ($env | keys), dllOverrides: ($ov | keys)}')
  if [ ! -f "$state" ] || [ "$(jq -c . "$state" 2>/dev/null)" != "$new_state" ]; then
    printf '%s\n' "$new_state" >"$state.tmp" && mv -f "$state.tmp" "$state"
  fi
  return 0
}

sync_prefix() {
  local name=$1 p="$PREFIXES/$1" want_runner want_arch cur_runner cur_arch problem verb comp type ver
  local -a verbs=() comps=()

  want_runner=$(jq -r --arg n "$name" '.prefixes[$n].runner' "$MANIFEST")
  want_arch=$(jq -r --arg n "$name" '.prefixes[$n].arch' "$MANIFEST")

  # A prefix that is not whole (or not a Cask prefix at all) is skipped: nothing in it is changed, and the
  # other prefixes and the runners and components are still handled. It is not an error of the run.
  if [ -e "$p" ]; then
    if [ -d "$p" ] && [ -z "$(ls -A "$p")" ]; then
      :
    elif [ ! -f "$p/cask.json" ]; then
      warn "prefix '$name': $p has no cask.json; skipped"
      return 0
    elif ! jq -e '(.runner | type == "string") and (.arch | type == "string")' "$p/cask.json" >/dev/null 2>&1; then
      warn "prefix '$name': cask.json is not usable; skipped"
      return 0
    elif problem=$(prefix_problem "$p"); then
      warn "prefix '$name' is broken ($problem); skipped. Fix it, or move it away to have it created again"
      return 0
    else
      cur_arch=$(jq -r '.arch' "$p/cask.json")
      if [ "$cur_arch" != "$want_arch" ]; then
        # The architecture cannot be changed in place.
        set_aside "$name" "old-$cur_arch" || return 1
      fi
    fi
  fi

  resolve_runner "$want_runner" || return 1

  if [ ! -f "$p/cask.json" ]; then
    log "creating prefix '$name' ($want_arch, runner '$want_runner')"
    mkdir -p "$p" && cask_json_create "$p" "$want_runner" "$want_arch" || return 1
    init_prefix "$p" "$want_arch" || return 1
  else
    cur_runner=$(jq -r '.runner // "system"' "$p/cask.json")
    if [ "$cur_runner" != "$want_runner" ]; then
      # PrefixManager::setRunner: stop the old wineserver, record the runner, wineboot -u.
      log "prefix '$name': runner '$cur_runner' -> '$want_runner'"
      stop_prefix "$p" "$cur_runner" "$want_arch"
      resolve_runner "$want_runner" || return 1
      cask_json_update "$p/cask.json" '.runner = $r' --arg r "$want_runner" || return 1
      if [ -d "$p/drive_c/windows/system32" ]; then
        prefix_env "$p" "$want_arch"
        env "${ENVV[@]}" "$WINE" wineboot -u || {
          err "prefix '$name': wineboot -u failed"
          return 1
        }
        env "${ENVV[@]}" "$WINESERVER" -w
        unlink_user_dirs "$p"
      fi
    fi
    # An earlier run was interrupted before the prefix was initialised.
    if [ ! -d "$p/drive_c/windows/system32" ]; then
      log "prefix '$name': finishing the initialisation"
      init_prefix "$p" "$want_arch" || return 1
    fi
  fi

  # winetricks first, the components after it: DXVK must win over whatever a verb installed.
  mapfile -t verbs < <(jq -r --arg n "$name" '.prefixes[$n].winetricks[]' "$MANIFEST")
  for verb in "${verbs[@]}"; do
    if verb_installed "$p" "$verb"; then
      continue
    fi
    log "prefix '$name': winetricks $verb"
    if ! run_verb "$p" "$want_arch" "$verb"; then
      err "prefix '$name': winetricks $verb failed; the verbs after it are not run"
      return 1
    fi
  done

  mapfile -t comps < <(jq -r --arg n "$name" '.prefixes[$n].components | to_entries[] | "\(.key)\t\(.value)"' "$MANIFEST")
  for comp in "${comps[@]}"; do
    type=${comp%%$'\t'*}
    ver=${comp#*$'\t'}
    if component_intact "$p" "$type" "$ver"; then
      continue
    fi
    apply_component "$p" "$type" "$ver" || return 1
  done

  # After the components: a DLL override of the configuration wins over the one a component has set.
  sync_settings "$name" "$p" "$want_arch" || return 1
}

# ---------------------------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------------------------

declare -a names=()

mapfile -t names < <(jq -r '.runners | keys[]' "$MANIFEST")
for n in "${names[@]}"; do
  link_managed "$ROOT/data/runners" "$n" "$(jq -r --arg n "$n" '.runners[$n]' "$MANIFEST")"
done
remove_stale "$ROOT/data/runners" runner "${names[@]}"

for type in dxvk d7vk vkd3d-proton; do
  mapfile -t names < <(jq -r --arg t "$type" '.components[$t] | keys[]' "$MANIFEST")
  for n in "${names[@]}"; do
    link_managed "$ROOT/data/$type" "$n" "$(jq -r --arg t "$type" --arg n "$n" '.components[$t][$n]' "$MANIFEST")"
  done
  remove_stale "$ROOT/data/$type" "$type" "${names[@]}"
done

mapfile -t names < <(jq -r '.prefixes | keys[]' "$MANIFEST")
for n in "${names[@]}"; do
  if ! sync_prefix "$n"; then
    FAILED=1
  fi
done

if [ "$FAILED" -ne 0 ]; then
  err "some of the steps failed (see above); run again to retry the rest"
  exit 1
fi
log "done"
