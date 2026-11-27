#!/usr/bin/env bash
# Runs gtest binaries of the Linux CI tree from a filter file and keeps the test
# launcher's summaries, the way olcumler/ada/test-kos.sh does for the mac tree.
# With --base it compares this run against an earlier one and fails only on
# tests that are red now and were not red there. With -r the launcher retries
# failed tests itself, one test at a time. With -R the failed tests of the first
# run go through a second launcher run with -R jobs in parallel. With -C the
# tests that would fail the run against the base run once more, one at a time,
# before the verdict. A test counts by its last run, and one that passes after
# failing is listed as flaky.
set -euo pipefail

OUT_NAME="ci"
DEFAULT_JOBS="$(getconf _NPROCESSORS_ONLN)"
DEFAULT_BATCH=1
DEFAULT_RETRY=0
DEFAULT_RERUN_JOBS=0
DEFAULT_CONFIRM_MAX=0
# These need the window focus to themselves, so the launcher runs them one by one.
SINGLE_JOB_BINARIES=" interactive_ui_tests "
# Tests of a binary that need the window focus to themselves as well: in a
# parallel run the window of another test takes the focus and their focus
# checks fail, while each passes run alone. The launcher runs the ones that
# the filter selects one by one after the parallel run of their binary, which
# leaves them out.
SINGLE_JOB_TESTS="
browser_tests DocumentPictureInPictureWindowControllerBrowserTest.CreateTwice
browser_tests FlagsUiBrowserTest.App
browser_tests TabViewTest.IconDataChanged
browser_tests CrElementsTest.CrToast
browser_tests BookmarksTest.List
browser_tests TabSearchTest.Item
browser_tests WebUiToolbarJsTest.ToolbarApp
browser_tests CrComponentsSearchboxTest.SearchboxIconTest
browser_tests VerticalTabStripRegionViewTest.ExpandOnHoverEnabledChanged
browser_tests VerticalTabStripRegionViewTest.ExpandOnHoverLockBehavior
browser_tests VerticalTabStripRegionViewTest.ModeChanged
browser_tests VerticalTabStripRegionViewTest.GetFocusedTabIndexReturnsCorrectIndex
browser_tests VerticalTabStripRegionViewTest.ResizeAreaNotVisibleInExpandOnHoverState
browser_tests VerticalTabStripRegionViewTest.ExpandOnHoverDisabledWhenWindowInactive
browser_tests VerticalTabStripRegionViewTest.LogExpandOnHoverShowDurationMetrics
browser_tests VerticalTabStripRegionViewTest.CanFocusVerticalTabView
browser_tests AdsPageLoadMetricsObserverBrowserTest.AdDensity_AdCreatedInBackgroundAccountedWhenTabRefocused
browser_tests AdsPageLoadMetricsObserverBrowserTest.AdDensity_AdCreatedInBackgroundNotAccountedWhileInBackground
browser_tests AdsPageLoadMetricsObserverBrowserTest.AdDensity_BackgroundedTimeNotAccounted
browser_tests AdsPageLoadMetricsObserverBrowserTest.DisplayNoneAdframeRecorded
browser_tests AdsPageLoadMetricsObserverBrowserTest.DocWriteAboutBlankAdframe
browser_tests AdsPageLoadMetricsObserverBrowserTest.FrameCreatedByAdScriptNavigatedToAllowListRule_NotRecorddedAsAd
browser_tests AdsPageLoadMetricsObserverBrowserTest.FramePixelSize
browser_tests AdsPageLoadMetricsObserverBrowserTest.FrameWithSmallAreaNotConsideredVisible
browser_tests AdsPageLoadMetricsObserverBrowserTest.OriginStatusMetricCross
browser_tests AdsPageLoadMetricsObserverBrowserTest.OriginStatusMetricEmbedded
browser_tests AdsPageLoadMetricsObserverBrowserTest.SubresourceFilter
browser_tests AdsPageLoadMetricsObserverBrowserTest.VisibleAdframeRecorded
browser_tests AdsPageLoadMetricsObserverBrowserTest.AdFrameRecordMediaStatusNotPlayed
browser_tests AdsPageLoadMetricsObserverBrowserTest.FirstContentfulPaintRecorded
browser_tests AdsPageLoadMetricsObserverBrowserTest.FrameDepth
browser_tests AdsPageLoadMetricsObserverBrowserTest.SameDomainFrameCreatedByAdScript_NotRecorddedAsAd
browser_tests AdsPageLoadMetricsObserverBrowserTest.OriginStatusMetricSame
browser_tests AdsPageLoadMetricsObserverBrowserTest.AggregateCpuTriggersCpuUpdateOverSubframeNavigate
browser_tests CreativeOriginAdsPageLoadMetricsObserverBrowserTest.CreativeOriginStatusSame
browser_tests CreativeOriginAdsPageLoadMetricsObserverBrowserTest.CreativeOriginStatusCross
browser_tests AdsPageLoadMetricsObserverBrowserTest.SubframeNavigate_CpuTimesCumulative
browser_tests CreativeOriginAdsPageLoadMetricsObserverBrowserTest.CreativeOriginStatusCrossNested
browser_tests CreativeOriginAdsPageLoadMetricsObserverBrowserTest.CreativeOriginStatusCrossDoubleNested
browser_tests CreativeOriginAdsPageLoadMetricsObserverBrowserTest.CreativeOriginStatusNoCreativeDesignated
browser_tests AdsPageLoadMetricsObserverPrerenderingBrowserTest.NoMetricsInPrerendering
browser_tests DevToolsAdsTest.GetAdMetrics
browser_tests DevToolsAdsTest.GetAdMetrics_AdFrames
browser_tests DevToolsAdsTest.GetAdMetrics_PageNavigated
browser_tests CaptureHandleBrowserTestPrerender.EmptyConfigForCrossDocumentNavigations
"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/out/$OUT_NAME"

usage() {
    echo "usage: core-test.sh [-j N] [-b N] [-r N] [-R N] [-C N] [--base DIR] FILTER_JSON RESULT_DIR"
    echo "FILTER_JSON  {\"<binary>\": \"<gtest_filter>\", ...}; \"*\" runs the whole suite;"
    echo "             keys starting with _ are ignored"
    echo "RESULT_DIR   created for this run, must not exist yet"
    echo "-j           launcher jobs, defaults to $DEFAULT_JOBS (interactive_ui_tests always 1)"
    echo "-b           launcher batch limit, defaults to $DEFAULT_BATCH (one test per process)"
    echo "-r           launcher retry limit for failed tests, defaults to $DEFAULT_RETRY;"
    echo "             the launcher runs its retries one test at a time"
    echo "-R           rerun the failed tests once more with N launcher jobs,"
    echo "             defaults to $DEFAULT_RERUN_JOBS (no rerun)"
    echo "-C           with --base, run the tests that are red now and were not red in"
    echo "             the base once more, one per process and one at a time, when there"
    echo "             are at most N of them; defaults to $DEFAULT_CONFIRM_MAX (no confirm run)"
    echo "--base DIR   an earlier RESULT_DIR; only tests red now and not red there fail the run"
}

die() { echo "core-test: $*" >&2; exit 2; }
say() { echo "core-test: $*"; }

jobs="$DEFAULT_JOBS"
batch="$DEFAULT_BATCH"
retry="$DEFAULT_RETRY"
rerun_jobs="$DEFAULT_RERUN_JOBS"
confirm_max="$DEFAULT_CONFIRM_MAX"
base=""
args=""
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        -j) shift; [ $# -gt 0 ] || die "-j needs a number"; jobs="$1" ;;
        -b) shift; [ $# -gt 0 ] || die "-b needs a number"; batch="$1" ;;
        -r) shift; [ $# -gt 0 ] || die "-r needs a number"; retry="$1" ;;
        -R) shift; [ $# -gt 0 ] || die "-R needs a number"; rerun_jobs="$1" ;;
        -C) shift; [ $# -gt 0 ] || die "-C needs a number"; confirm_max="$1" ;;
        --base) shift; [ $# -gt 0 ] || die "--base needs a directory"; base="$1" ;;
        -*) die "unknown option: $1" ;;
        *) args="${args:+$args
}$1" ;;
    esac
    shift
done
case "$jobs" in ''|*[!0-9]*) die "-j must be a number, got: $jobs" ;; esac
case "$batch" in ''|*[!0-9]*) die "-b must be a number, got: $batch" ;; esac
case "$retry" in ''|*[!0-9]*) die "-r must be a number, got: $retry" ;; esac
case "$rerun_jobs" in ''|*[!0-9]*) die "-R must be a number, got: $rerun_jobs" ;; esac
case "$confirm_max" in ''|*[!0-9]*) die "-C must be a number, got: $confirm_max" ;; esac

filter_json="$(printf '%s\n' "$args" | sed -n '1p')"
result="$(printf '%s\n' "$args" | sed -n '2p')"
[ -n "$filter_json" ] && [ -n "$result" ] || { usage; exit 2; }
[ "$(printf '%s\n' "$args" | wc -l | tr -d ' ')" = "2" ] || die "expected FILTER_JSON and RESULT_DIR, got more"
[ -f "$filter_json" ] || die "no filter file at $filter_json"
[ ! -e "$result" ] || die "$result exists; each run writes into a new directory"
if [ -n "$base" ]; then
    [ -d "$base" ] || die "no earlier run at $base"
fi
[ "$confirm_max" -eq 0 ] || [ -n "$base" ] || die "-C needs --base: it confirms the tests that are new against the base"
[ -f "$ROOT/testing/xvfb.py" ] || die "no testing/xvfb.py in the tree"
# xvfb.py is written for vpython3 and imports psutil, the one module it needs
# beyond the standard library. The CI host runs it with the system python3, so
# the distribution's package stands in for the vpython wheel.
python3 -c 'import psutil' 2>/dev/null \
    || die "python3 has no psutil, which testing/xvfb.py imports; install it once: sudo apt-get install -y python3-psutil"

list="$(python3 - "$filter_json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
assert isinstance(d, dict), "the filter file must hold one object"
for binary, flt in d.items():
    if binary.startswith("_"):
        continue
    assert isinstance(flt, str) and flt, "filter for %s must be a non-empty string" % binary
    print("%s\t%s" % (binary, flt))
PY
)" || die "could not read $filter_json"
[ -n "$list" ] || die "no binaries in $filter_json"

while IFS="$(printf '\t')" read -r binary filter; do
    [ -x "$OUT/$binary" ] || die "no $binary in out/$OUT_NAME; build it first"
done <<EOF
$list
EOF

# Runs BINARY under xvfb.py with FILTER and the launcher flags that follow, the
# summary going to SUMMARY_PATH and the output to LOG.
# xvfb.py starts a virtual X server (with openbox and xcompmgr) for the binary;
# install-build-deps.py puts those packages in place. Its default backing is
# Xorg, which also needs a dummy video driver set up; --use-xvfb keeps the plain
# Xvfb server, so every run gets the same display.
# The binary runs with CHROME_CONFIG_HOME set to a new empty directory next to
# SUMMARY_PATH. Chrome puts its default user data directory there instead of
# ~/.config (chrome/common/chrome_paths_linux.cc), so what one launch writes
# there, the profiles of crashed tests included, stays in RESULT_DIR.
# PATH starts with vpython_shim, the vpython3 shim directory of the run.
launch() {
    local binary="$1" filter="$2" summary_path="$3" log="$4"
    shift 4
    local config_home="$summary_path.config"
    mkdir "$config_home"
    ( cd "$OUT" && CHROME_CONFIG_HOME="$config_home" \
        PATH="$vpython_shim:$PATH" \
        python3 "$ROOT/testing/xvfb.py" "./$binary" --use-xvfb \
        --gtest_filter="$filter" "$@" \
        --test-launcher-summary-output="$summary_path" ) > "$log" 2>&1
}

# Writes OUT from the launcher summaries that follow, in the order they ran:
# each test keeps the runs of every summary, so its last run decides its status
# and a failure before it marks the test flaky. Without the first summary it
# writes nothing; a later one that is missing is left out. OUT may be the first.
merge() {
    python3 - "$@" <<'PY'
import json, os, sys
out, first, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
if not os.path.isfile(first):
    sys.exit(0)
data = json.load(open(first))
merged = data.setdefault("per_iteration_data", [{}])
if not merged:
    merged.append({})
for path in rest:
    if not os.path.isfile(path):
        continue
    for iteration in json.load(open(path)).get("per_iteration_data", []):
        for name, runs in iteration.items():
            merged[0].setdefault(name, []).extend(runs)
json.dump(data, open(out, "w"))
PY
}

# Compares the merged summaries (*.json) of RESULT with those of BASE.
#   compare candidates RESULT BASE: prints "<binary>\t<test>" for each test that
#     is red now and was not red in BASE, a sub-test given as its test.
#   compare verdict RESULT BASE MISSING: prints the verdict and exits 1 on RED.
# Without BASE any red test fails the run; with it only the tests that are red
# now and were not red there do. A binary without a summary always fails it:
# there is nothing to compare.
compare() {
    python3 - "$@" <<'PY'
import glob, json, os, sys
mode, result, base = sys.argv[1:4]
ok = ("SUCCESS", "SKIPPED")

def statuses(directory):
    out = {}
    for path in glob.glob(os.path.join(directory, "*.json")):
        binary = os.path.basename(path)[:-len(".json")]
        for iteration in json.load(open(path)).get("per_iteration_data", []):
            for name, runs in iteration.items():
                out[(binary, name)] = runs[-1]["status"] if runs else "UNKNOWN"
    return out

now = statuses(result)
before = statuses(base) if base else {}
red_now = sorted(k for k, s in now.items() if s not in ok)
new_red = [k for k in red_now if before.get(k, "ABSENT") in ok + ("ABSENT",)]

if mode == "candidates":
    # The launcher lists each sub-test as "<test>__<sub-test>" next to its test
    # (SubTestResult::FullName, base/test/launcher/test_result.cc); a gtest
    # filter selects only the test, whose run reports the sub-tests again.
    tests = set()
    for binary, name in new_red:
        test, start = name, 0
        while True:
            i = name.find("__", start)
            if i < 0:
                break
            if (binary, name[:i]) in now:
                test = name[:i]
                break
            start = i + 1
        tests.add((binary, test))
    for binary, test in sorted(tests):
        print("%s\t%s" % (binary, test))
    sys.exit(0)

missing = sys.argv[4] == "1"
if base:
    fixed = sorted(k for k, s in before.items() if s not in ok and now.get(k) in ok)
    print("=== verdict vs %s: red now %d, new red %d, fixed %d" % (base, len(red_now), len(new_red), len(fixed)))
    for binary, name in new_red:
        print("NEW RED %s %s (%s, before %s)" % (binary, name, now[(binary, name)], before.get((binary, name), "ABSENT")))
    failed = bool(new_red)
else:
    print("=== verdict without a base: red %d" % len(red_now))
    failed = bool(red_now)
if missing:
    print("=== a binary, its rerun, its one-at-a-time run or its confirm run left no summary json")
print("=== %s" % ("RED" if failed or missing else "GREEN"))
sys.exit(1 if failed or missing else 0)
PY
}

# Prints, joined with ":", the SINGLE_JOB_TESTS of BINARY that FILTER selects.
single_job_tests() {
    python3 - "$1" "$2" "$SINGLE_JOB_TESTS" <<'PY'
import fnmatch, sys
binary, flt, tests = sys.argv[1:4]
positive, _, negative = flt.partition("-")
positive = [p for p in positive.split(":") if p] or ["*"]
negative = [p for p in negative.split(":") if p]
names = []
for line in tests.split("\n"):
    fields = line.split()
    if len(fields) != 2 or fields[0] != binary:
        continue
    if any(fnmatch.fnmatchcase(fields[1], p) for p in positive) and not any(
            fnmatch.fnmatchcase(fields[1], p) for p in negative):
        names.append(fields[1])
print(":".join(names))
PY
}

mkdir -p "$result"
# The binary runs from out/ci, so a relative RESULT_DIR would put its summary
# json there instead of here.
result="$(cd "$result" && pwd)"
# Chrome tests start vpython3 by name; media_engagement_autoplay_browsertest.cc
# runs make_dafsa.py with it. The CI host has no vpython3 on PATH, and the test
# process sets XDG_CACHE_HOME to its own new user data directory
# (chrome/test/base/test_launcher_utils.cc), where a depot_tools vpython3 would
# set up its environment from nothing in each test. The binaries find a
# vpython3 here that is the python3 of this script.
vpython_shim="$result/vpython3-shim"
mkdir "$vpython_shim"
ln -s "$(command -v python3)" "$vpython_shim/vpython3"
summary="$result/ozet.txt"
head="$(git -C "$ROOT" rev-parse --short=10 HEAD 2>/dev/null || echo unknown)"
{
    echo "=== core-test $(date '+%Y-%m-%d %H:%M:%S') HEAD $head, -j $jobs, -b $batch, -r $retry, -R $rerun_jobs, -C $confirm_max"
    echo "filter $filter_json${base:+, base $base}"
} | tee "$summary"

missing=0
while IFS="$(printf '\t')" read -r binary filter; do
    j="$jobs"
    case "$SINGLE_JOB_BINARIES" in *" $binary "*) j=1 ;; esac
    started=$SECONDS
    say "$binary, filter ${#filter} characters, -j $j" | tee -a "$summary"
    rc=0
    single="$(single_job_tests "$binary" "$filter")"
    main_filter="$filter"
    if [ -n "$single" ]; then
        case "$filter" in
            *-*) main_filter="$filter:$single" ;;
            *) main_filter="$filter-$single" ;;
        esac
    fi
    # The launcher's own summaries keep a .summary name so that the verdict,
    # which reads every *.json here, only sees the merged one.
    launch "$binary" "$main_filter" "$result/$binary.main.summary" "$result/$binary.log" \
        --test-launcher-retry-limit="$retry" \
        --test-launcher-batch-limit="$batch" \
        --test-launcher-jobs="$j" || rc=$?
    if [ "$rerun_jobs" -gt 0 ] && [ -f "$result/$binary.main.summary" ]; then
        failed="$(python3 - "$result/$binary.main.summary" <<'PY'
import json, sys
names = set()
for iteration in json.load(open(sys.argv[1])).get("per_iteration_data", []):
    for name, runs in iteration.items():
        if runs and runs[-1]["status"] not in ("SUCCESS", "SKIPPED"):
            names.add(name)
print(":".join(sorted(names)))
PY
)"
        if [ -n "$failed" ]; then
            say "$binary, rerun of the failed tests with -j $rerun_jobs" | tee -a "$summary"
            launch "$binary" "$failed" "$result/$binary.rerun.summary" "$result/$binary.rerun.log" \
                --test-launcher-retry-limit=0 \
                --test-launcher-batch-limit="$batch" \
                --test-launcher-jobs="$rerun_jobs" || true
            [ -f "$result/$binary.rerun.summary" ] \
                || { say "$binary, the rerun left no summary" | tee -a "$summary"; missing=1; }
        fi
    fi
    if [ -n "$single" ]; then
        say "$binary, $(printf '%s\n' "$single" | tr ':' '\n' | grep -c .) tests that need the window focus, one at a time" \
            | tee -a "$summary"
        launch "$binary" "$single" "$result/$binary.single.summary" "$result/$binary.single.log" \
            --test-launcher-retry-limit="$retry" \
            --test-launcher-batch-limit=1 \
            --test-launcher-jobs=1 || true
        [ -f "$result/$binary.single.summary" ] \
            || { say "$binary, the one-at-a-time run left no summary" | tee -a "$summary"; missing=1; }
    fi
    merge "$result/$binary.json" "$result/$binary.main.summary" "$result/$binary.rerun.summary" \
        "$result/$binary.single.summary" || true
    python3 - "$result/$binary.json" "$binary" "$rc" "$(( SECONDS - started ))" <<'PY' | tee -a "$summary" || missing=1
import collections, json, os, sys
path, binary, rc, secs = sys.argv[1:5]
if not os.path.isfile(path):
    print("--- %s rc %s, %ss: NO SUMMARY JSON" % (binary, rc, secs))
    sys.exit(1)
data = json.load(open(path))
count = collections.Counter()
red = []
flaky = []
for iteration in data.get("per_iteration_data", []):
    for name, runs in iteration.items():
        status = runs[-1]["status"] if runs else "UNKNOWN"
        count[status] += 1
        if status not in ("SUCCESS", "SKIPPED"):
            red.append("%s %s" % (status, name))
        elif any(r["status"] not in ("SUCCESS", "SKIPPED") for r in runs[:-1]):
            flaky.append("%s %s" % (",".join(r["status"] for r in runs[:-1]), name))
print("--- %s rc %s, %ss: %s%s" % (binary, rc, secs,
      ", ".join("%s %d" % kv for kv in sorted(count.items())) or "no tests",
      ", flaky %d" % len(flaky) if flaky else ""))
for line in red[:40]:
    print("RED " + line)
if len(red) > 40:
    print("RED ... and %d more" % (len(red) - 40))
for line in flaky[:40]:
    print("FLAKY " + line)
if len(flaky) > 40:
    print("FLAKY ... and %d more" % (len(flaky) - 40))
PY
done <<EOF
$list
EOF

# The confirm run. A test that would fail the run against the base runs once
# more in a process of its own, with no other test beside it: one that fails
# only under the load of the full suite passes here and stays listed as flaky,
# one that is broken fails again. More than -C such tests is no load noise, so
# the run fails on them without a confirm run.
if [ "$confirm_max" -gt 0 ]; then
    candidates="$(compare candidates "$result" "$base")"
    count="$(printf '%s' "$candidates" | grep -c . || true)"
    if [ "$count" -eq 0 ]; then
        say "confirm: no new red test" | tee -a "$summary"
    elif [ "$count" -gt "$confirm_max" ]; then
        say "confirm: $count new red tests, more than -C $confirm_max; no confirm run" | tee -a "$summary"
    else
        say "confirm: $count new red tests, one at a time" | tee -a "$summary"
        for binary in $(printf '%s\n' "$candidates" | cut -f1 | sort -u); do
            filter="$(printf '%s\n' "$candidates" | awk -F '\t' -v b="$binary" '$1 == b { print $2 }' | paste -sd: -)"
            launch "$binary" "$filter" "$result/$binary.confirm.summary" "$result/$binary.confirm.log" \
                --test-launcher-retry-limit=0 \
                --test-launcher-batch-limit=1 \
                --test-launcher-jobs=1 || true
            if [ -f "$result/$binary.confirm.summary" ]; then
                merge "$result/$binary.json" "$result/$binary.json" "$result/$binary.confirm.summary"
                python3 - "$result/$binary.confirm.summary" "$binary" "$filter" <<'PY' | tee -a "$summary"
import json, sys
path, binary, flt = sys.argv[1:4]
last = {}
for iteration in json.load(open(path)).get("per_iteration_data", []):
    for name, runs in iteration.items():
        if runs:
            last[name] = runs[-1]["status"]
for name in flt.split(":"):
    print("CONFIRM %s %s %s" % (binary, last.get(name, "NOT RUN"), name))
PY
            else
                say "confirm: $binary left no summary" | tee -a "$summary"
                missing=1
            fi
        done
    fi
fi

compare verdict "$result" "$base" "$missing" | tee -a "$summary"
