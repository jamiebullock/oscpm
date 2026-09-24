#!/usr/bin/env python3
# Part of oscpm
#
# SPDX-FileCopyrightText: 2026 Jamie Bullock
# SPDX-License-Identifier: Zlib

"""Runs the oscpm benchmarks and compares two revisions.

instructions  instructions per iteration under Cachegrind, base against head;
              exits 1 when a benchmark regresses past the threshold
time          wall-clock, base against head, in interleaved rounds
report        wall-clock medians of the headline benchmarks, as Markdown
"""

import argparse
import concurrent.futures
import datetime
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

kRoot = Path(__file__).resolve().parent.parent
kRegressionRatio = 1.02
kRegressionFloor = 4
kProbeNoiseBudget = 100000
kMaxIterations = 500
kRegexSpecials = set(".^$|()[]{}*+?\\")
kHeadline = [
    ("Dispatch/repeat/literal/large", "Repeated literal address"),
    ("Dispatch/repeat/example/large", "Repeated `/synth[3-6]/voice/*/osc/{saw,square}/freq`, 128 methods"),
    ("Dispatch/repeat/descendant-freq/large", "Repeated `//freq`, 512 methods"),
    ("Dispatch/stream/large", "One message of a repeating stream of 64"),
    ("Dispatch/cold/literal/large", "First lookup, literal address"),
    ("Dispatch/cold/example/large", "First lookup, `/synth[3-6]/voice/*/osc/{saw,square}/freq`"),
    ("Dispatch/cold/descendant-freq/large", "First lookup, `//freq`"),
    ("Match/prepared/literal-hit", "`Pattern::matches`, literal"),
    ("Match/prepared/example-hit", "`Pattern::matches`, wildcards"),
    ("Match/parse/example-hit", "`oscpm::match`: parse and match, wildcards"),
    ("Dispatch/rejected/large", "A 64 KB wildcard pattern, rejected"),
]
kWorstAdversarialFilter = "Dispatch/adversarial/.*-1024/large"


def run(command, **kwargs):
    return subprocess.run(command, check=True, text=True, **kwargs)


def anchored(name):
    return "^" + "".join("\\" + c if c in kRegexSpecials else c for c in name) + "$"


def git(*arguments):
    return run(["git", "-C", str(kRoot), *arguments], capture_output=True).stdout.strip()


def configureAndBuild(sourceDir, buildDir, workDir, includeDir=None):
    command = [
        "cmake", "--preset", "bench", "-B", str(buildDir),
        "-DOSCPM_BUILD_TESTS=OFF", "-DOSCPM_BUILD_EXAMPLES=OFF", "-DOSCPM_CHECK_FORMAT=OFF",
        f"-DFETCHCONTENT_BASE_DIR={workDir / '_deps'}",
    ]
    if includeDir is not None:
        command.append(f"-DOSCPM_BENCH_INCLUDE_DIR={includeDir}")
    try:
        run(command, cwd=sourceDir, capture_output=True)
        run(["cmake", "--build", str(buildDir), "--target", "oscpm_bench"], capture_output=True)
    except subprocess.CalledProcessError as error:
        print(error.stdout, error.stderr, sep="\n", file=sys.stderr)
        return None
    return buildDir / "bench" / "oscpm_bench"


def checkOutBase(revision, workDir):
    baseSource = workDir / "base-src"
    shutil.rmtree(baseSource, ignore_errors=True)
    baseSource.mkdir(parents=True)
    archive = subprocess.run(["git", "-C", str(kRoot), "archive", revision], check=True, capture_output=True).stdout
    subprocess.run(["tar", "-x", "-m", "-C", str(baseSource)], input=archive, check=True)
    return baseSource


def buildBase(revision, workDir):
    baseSource = checkOutBase(revision, workDir)
    print(f"building head's benchmarks against {revision}'s headers", file=sys.stderr)
    binary = configureAndBuild(kRoot, workDir / "base", workDir, baseSource / "include")
    if binary is not None:
        return binary
    if not (baseSource / "bench" / "CMakeLists.txt").exists():
        print(f"{revision} has no benchmarks; reporting head alone", file=sys.stderr)
        return None
    print(f"building {revision}'s own benchmarks", file=sys.stderr)
    shutil.rmtree(workDir / "base", ignore_errors=True)
    return configureAndBuild(baseSource, workDir / "base", workDir)


def benchmarkNames(binary, pattern):
    listed = run([str(binary), "--benchmark_list_tests"], capture_output=True).stdout.split()
    return [name for name in listed if pattern is None or re.search(pattern, name)]


def iterationsFor(estimate):
    return max(1, min(kMaxIterations, -(-kProbeNoiseBudget // max(1, int(estimate)))))


def instructionsAt(binary, name, iterations):
    completed = run([
        "valgrind", "--tool=cachegrind", "--cache-sim=no", "--cachegrind-out-file=/dev/null",
        str(binary), f"--benchmark_filter={anchored(name)}", f"--benchmark_min_time={iterations}x",
    ], capture_output=True)
    found = re.search(r"I\s+refs:\s+([\d,]+)", completed.stderr)
    if found is None:
        raise RuntimeError(f"no instruction count for {name}:\n{completed.stderr}")
    return int(found.group(1).replace(",", ""))


def instructionsPerIteration(binary, name, iterations):
    return (instructionsAt(binary, name, 2 * iterations) - instructionsAt(binary, name, iterations)) / iterations


def probedIterations(binary, name):
    return iterationsFor(instructionsPerIteration(binary, name, 1))


def inParallel(function, names, jobs):
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        return dict(zip(names, pool.map(function, names)))


def countInstructions(binary, iterations, jobs):
    return inParallel(lambda name: instructionsPerIteration(binary, name, iterations[name]), list(iterations), jobs)


def isRegression(base, head):
    return head > base * kRegressionRatio and head - base > kRegressionFloor


def instructionTable(base, head):
    lines = ["| Benchmark | Base | Head | Head / base |", "|---|---:|---:|---:|"]
    regressions = []
    for name in sorted(set(base) | set(head)):
        if name not in head:
            lines.append(f"| {name} | {base[name]:,.0f} | removed | |")
            continue
        if name not in base:
            lines.append(f"| {name} | new | {head[name]:,.0f} | |")
            continue
        ratio = head[name] / base[name] if base[name] > 0 else float("inf")
        marker = ""
        if isRegression(base[name], head[name]):
            regressions.append(name)
            marker = " **regressed**"
        lines.append(f"| {name} | {base[name]:,.0f} | {head[name]:,.0f} | {ratio:.3f}{marker} |")
    return lines, regressions


def writeSummary(path, lines):
    text = "\n".join(lines) + "\n"
    print(text)
    if path:
        with open(path, "a", encoding="utf-8") as summary:
            summary.write(text)


def compareInstructions(arguments, workDir):
    headBinary = configureAndBuild(kRoot, workDir / "head", workDir)
    if headBinary is None:
        sys.exit("head does not build")
    baseBinary = buildBase(arguments.base, workDir)
    names = benchmarkNames(headBinary, arguments.filter)
    iterations = inParallel(lambda name: probedIterations(headBinary, name), names, arguments.jobs)
    head = countInstructions(headBinary, iterations, arguments.jobs)
    base = {}
    if baseBinary is not None:
        baseNames = benchmarkNames(baseBinary, arguments.filter)
        baseIterations = {name: iterations.get(name) or probedIterations(baseBinary, name) for name in baseNames}
        base = countInstructions(baseBinary, baseIterations, arguments.jobs)
    (workDir / "instructions-head.json").write_text(json.dumps(head, indent=1))
    (workDir / "instructions-base.json").write_text(json.dumps(base, indent=1))

    heading = f"## Instructions per iteration, {arguments.base} against head"
    if not base:
        lines = [heading, "", "| Benchmark | Head |", "|---|---:|"]
        lines += [f"| {name} | {head[name]:,.0f} |" for name in sorted(head)]
        writeSummary(arguments.summary, lines)
        return 0
    table, regressions = instructionTable(base, head)
    verdict = (f"{len(regressions)} benchmark(s) rose by more than {kRegressionRatio - 1:.0%} "
               f"and {kRegressionFloor} instructions: {', '.join(regressions)}"
               if regressions else "No benchmark regressed.")
    writeSummary(arguments.summary, [heading, "", verdict, "", *table])
    return 1 if regressions and not arguments.allowRegressions else 0


def timesByName(binary, filterPattern, extraArguments):
    command = [str(binary), "--benchmark_format=json", *extraArguments]
    if filterPattern:
        command.append(f"--benchmark_filter={filterPattern}")
    report = json.loads(run(command, capture_output=True).stdout)
    scale = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}
    return report, {
        entry["run_name"]: timePerItem(entry, scale)
        for entry in report["benchmarks"]
        if entry.get("aggregate_name", "median") == "median"
    }


def timePerItem(entry, scale):
    return entry["real_time"] * scale[entry["time_unit"]] / entry.get("messages", 1)


def formatTime(nanoseconds):
    for unit, size in (("s", 1e9), ("ms", 1e6), ("us", 1e3)):
        if nanoseconds >= size:
            return f"{nanoseconds / size:.3g} {unit}"
    return f"{nanoseconds:.3g} ns"


def compareTimes(arguments, workDir):
    headBinary = configureAndBuild(kRoot, workDir / "head", workDir)
    baseBinary = buildBase(arguments.base, workDir)
    if headBinary is None or baseBinary is None:
        sys.exit("both revisions must build their benchmarks")
    extra = [f"--benchmark_min_time={arguments.minTime}"]
    rounds = {"base": [], "head": []}
    for index in range(arguments.rounds):
        order = [("base", baseBinary), ("head", headBinary)]
        for side, binary in order if index % 2 == 0 else reversed(order):
            print(f"round {index + 1}: {side}", file=sys.stderr)
            rounds[side].append(timesByName(binary, arguments.filter, extra)[1])
    lines = [f"## Wall-clock, {arguments.base} against head, median of {arguments.rounds} rounds", "",
             "| Benchmark | Base | Head | Head / base |", "|---|---:|---:|---:|"]
    for name in sorted(set(rounds["head"][0]) & set(rounds["base"][0])):
        base = statistics.median(r[name] for r in rounds["base"])
        head = statistics.median(r[name] for r in rounds["head"])
        ratio = statistics.median(h[name] / b[name] for b, h in zip(rounds["base"], rounds["head"]))
        lines.append(f"| {name} | {formatTime(base)} | {formatTime(head)} | {ratio:.2f} |")
    writeSummary(arguments.summary, lines)
    return 0


def processorName():
    if sys.platform == "darwin":
        return output(["sysctl", "-n", "machdep.cpu.brand_string"])
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        found = re.search(r"model name\s*:\s*(.+)", cpuinfo.read_text())
        if found:
            return found.group(1).strip()
    return platform.processor() or platform.machine()


def operatingSystem():
    if sys.platform == "darwin":
        return "macOS " + output(["sw_vers", "-productVersion"])
    return platform.platform(terse=True)


def output(command):
    return run(command, capture_output=True).stdout.strip()


def cachedFlags(buildDir):
    cache = (buildDir / "CMakeCache.txt").read_text()
    found = re.search(r"^CMAKE_CXX_FLAGS_RELEASE:STRING=(.*)$", cache, re.MULTILINE)
    return found.group(1) if found else ""


def libraryVersion():
    tag = git("describe", "--tags", "--abbrev=0")
    if git("diff", "--name-only", tag, "--", "include") == "":
        return tag
    return git("describe", "--tags", "--dirty")


def report(arguments, workDir):
    binary = configureAndBuild(kRoot, workDir / "head", workDir)
    if binary is None:
        sys.exit("head does not build")
    extra = [f"--benchmark_repetitions={arguments.repetitions}", "--benchmark_report_aggregates_only=true",
             f"--benchmark_min_time={arguments.minTime}"]
    headlineFilter = "|".join(anchored(name) for name, _ in kHeadline)
    context, medians = timesByName(binary, headlineFilter, extra)
    _, adversarial = timesByName(binary, kWorstAdversarialFilter, extra)
    worst = max(adversarial, key=adversarial.get)
    shape = worst.split("/")[2].rsplit("-", 1)[0]

    rows = [(name, description) for name, description in kHeadline if name != "Dispatch/rejected/large"]
    rows.append((worst, f"The slowest hostile pattern of 1024 bytes (`{shape}`)"))
    rows.append(("Dispatch/rejected/large", dict(kHeadline)["Dispatch/rejected/large"]))
    medians.update(adversarial)

    compiler = context["context"].get("compiler", "unknown compiler")
    version = libraryVersion()
    lines = [
        f"{processorName()}, {operatingSystem()}, {compiler} `{cachedFlags(workDir / 'head')}`, "
        f"oscpm {version}, {datetime.date.today().isoformat()}. Median of {arguments.repetitions} "
        f"repetitions; dispatch into the 1,856-method space.",
        "",
        "| Operation | Median |",
        "|---|---:|",
    ]
    lines += [f"| {description} | {formatTime(medians[name])} |" for name, description in rows]
    writeSummary(arguments.summary, lines)
    return 0


def defaultBase():
    for candidate in ("origin/develop", "develop"):
        try:
            return git("rev-parse", "--short", git("merge-base", candidate, "HEAD"))
        except subprocess.CalledProcessError:
            continue
    sys.exit("no develop branch to compare against; pass --base")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mode", choices=["instructions", "time", "report"])
    parser.add_argument("--base", help="revision to compare against (default: merge-base with develop)")
    parser.add_argument("--filter", help="regular expression selecting benchmarks by name")
    parser.add_argument("--work-dir", dest="workDir", type=Path, default=kRoot / "build" / "compare")
    parser.add_argument("--summary", default=os.environ.get("GITHUB_STEP_SUMMARY"),
                        help="file the Markdown result is appended to")
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    parser.add_argument("--allow-regressions", dest="allowRegressions", action="store_true",
                        help="report regressions without failing")
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=10)
    parser.add_argument("--min-time", dest="minTime", default="0.2s")
    arguments = parser.parse_args()

    workDir = arguments.workDir.resolve()
    workDir.mkdir(parents=True, exist_ok=True)
    if arguments.mode != "report" and arguments.base is None:
        arguments.base = defaultBase()
    if arguments.mode == "instructions":
        return compareInstructions(arguments, workDir)
    if arguments.mode == "time":
        return compareTimes(arguments, workDir)
    return report(arguments, workDir)


if __name__ == "__main__":
    sys.exit(main())
