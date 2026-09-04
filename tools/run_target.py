#!/usr/bin/env python3
"""Launch a built distribution artifact.

    python3 tools/run_target.py dist/ps2pal/engine.iso
    python3 tools/run_target.py dist/vita/PSEN00001.vpk
    python3 tools/run_target.py dist/win32/game.exe [launcher]

The emulator is chosen from the artifact extension, so no caller has to name
one. See docs/CLION_SETUP.md for the environment overrides.
"""

import os
import shutil
import subprocess
import sys

PCSX2 = "pcsx2"
VITA3K = "vita3k"
NATIVE = "native"

LAUNCHER_BY_EXTENSION = {
    ".iso": PCSX2,
    ".vpk": VITA3K,
    ".exe": NATIVE,
}

ENV_OVERRIDE = {
    PCSX2: "PCSX2_PATH",
    VITA3K: "VITA3K_PATH",
}

SEARCH_PATHS = {
    PCSX2: {
        "darwin": ["/Applications/PCSX2.app/Contents/MacOS/PCSX2"],
        "wsl": [
            "/mnt/c/PCSX2/pcsx2-qt.exe",
            "/c/PCSX2/pcsx2-qt.exe",
            "/mnt/c/Program Files/PCSX2/pcsx2-qt.exe",
            "/c/Program Files/PCSX2/pcsx2-qt.exe",
        ],
        "win32": [
            "C:\\PCSX2\\pcsx2-qt.exe",
            "C:\\Program Files\\PCSX2\\pcsx2-qt.exe",
        ],
        "posix": [],
    },
    VITA3K: {
        "darwin": ["/Applications/Vita3K.app/Contents/MacOS/Vita3K"],
        "wsl": [
            "/mnt/c/Vita3K/Vita3K.exe",
            "/c/Vita3K/Vita3K.exe",
            "/mnt/c/Program Files/Vita3K/Vita3K.exe",
            "/c/Program Files/Vita3K/Vita3K.exe",
        ],
        "win32": [
            "C:\\Vita3K\\Vita3K.exe",
            "C:\\Program Files\\Vita3K\\Vita3K.exe",
        ],
        "posix": [
            os.path.expanduser("~/.local/share/Vita3K/Vita3K"),
            os.path.expanduser("~/Vita3K/Vita3K"),
        ],
    },
}

ON_PATH = {
    PCSX2: ["pcsx2-qt", "pcsx2"],
    VITA3K: ["Vita3K", "vita3k"],
}


class RunError(Exception):
    """A launch that could not be attempted, with an actionable message."""


def is_wsl():
    """@return Whether this interpreter is running inside WSL."""
    try:
        with open("/proc/version", "r", encoding="utf-8", errors="replace") as fh:
            return "microsoft" in fh.read().lower()
    except OSError:
        return False


def host_kind():
    """@return One of "darwin", "win32", "wsl" or "posix"."""
    if sys.platform == "darwin":
        return "darwin"
    if sys.platform == "win32":
        return "win32"
    return "wsl" if is_wsl() else "posix"


def launcher_for(artifact):
    """Choose a launcher from the artifact extension.

    @param artifact Path to a built distribution artifact.
    @return One of the launcher constants.
    @raise RunError When the extension is not one this project produces.
    """
    ext = os.path.splitext(artifact)[1].lower()
    if ext not in LAUNCHER_BY_EXTENSION:
        known = ", ".join(sorted(LAUNCHER_BY_EXTENSION))
        raise RunError(f"do not know how to launch '{os.path.basename(artifact)}' (known extensions: {known})")
    return LAUNCHER_BY_EXTENSION[ext]


def resolve_launcher(launcher, explicit=None, host=None):
    """Locate the program that runs this kind of artifact.

    Order: an explicit path, then the environment override, then the per-host
    search paths, then PATH.

    @param launcher One of the launcher constants.
    @param explicit Path supplied by the caller, or None.
    @param host Host kind; defaults to the running host.
    @return An absolute path, or the artifact itself for a native target.
    @raise RunError When nothing was found, naming every path searched.
    """
    if launcher == NATIVE:
        return None

    host = host or host_kind()
    tried = []

    if explicit:
        if os.path.exists(explicit) or shutil.which(explicit):
            return explicit
        tried.append(explicit)

    override = os.environ.get(ENV_OVERRIDE[launcher])
    if override:
        if os.path.exists(override) or shutil.which(override):
            return override
        tried.append(f"{override}  (from ${ENV_OVERRIDE[launcher]})")

    for candidate in SEARCH_PATHS[launcher].get(host, []):
        if os.path.exists(candidate):
            return candidate
        tried.append(candidate)

    for name in ON_PATH[launcher]:
        found = shutil.which(name)
        if found:
            return found
        tried.append(f"{name}  (on PATH)")

    listing = "\n".join(f"    {t}" for t in tried)
    raise RunError(
        f"{launcher} not found. Looked in:\n{listing}\n"
        f"    Set ${ENV_OVERRIDE[launcher]} to its location, or pass it as the second argument."
    )


def host_path(path, host=None):
    """Translate a path into the form the launcher will understand.

    @param path Absolute path on this host.
    @param host Host kind; defaults to the running host.
    @return A Windows-form path under WSL, otherwise the path unchanged.
    """
    host = host or host_kind()
    if host != "wsl":
        return path
    try:
        return subprocess.check_output(["wslpath", "-w", path], text=True).strip()
    except (subprocess.CalledProcessError, FileNotFoundError, OSError):
        return path


def build_command(launcher, program, artifact, host=None):
    """Assemble the argument vector that launches the artifact.

    @param launcher One of the launcher constants.
    @param program The resolved launcher path, or None for a native target.
    @param artifact Path to the artifact, already host-translated.
    @param host Host kind; defaults to the running host.
    @return The argument vector to spawn.
    """
    host = host or host_kind()

    if launcher == NATIVE:
        return [artifact]

    if launcher == PCSX2:
        cmd = [program]
        if host != "darwin":
            cmd.append("-portable")
        cmd += ["-batch", artifact]
        return cmd

    # Vita3K takes the package as a positional argument, which installs it and
    # then runs it. There is no --run option; -r takes an installed title id.
    return [program, artifact]


def spawn(cmd):
    """Start the launcher detached from this process.

    @param cmd Argument vector to spawn.
    """
    if sys.platform == "win32":
        subprocess.Popen(cmd, creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
    else:
        subprocess.Popen(cmd, start_new_session=True)


def run(artifact, explicit=None, spawner=spawn):
    """Resolve a launcher for the artifact and start it.

    @param artifact Path to a built distribution artifact.
    @param explicit Launcher path supplied by the caller, or None.
    @param spawner Callable receiving the argument vector; substituted by tests.
    @return The argument vector that was spawned.
    @raise RunError When the artifact is missing, or no launcher was found.
    """
    artifact = os.path.abspath(artifact)
    if not os.path.exists(artifact):
        raise RunError(f"no artifact at {artifact}. Build it first.")

    launcher = launcher_for(artifact)
    host = host_kind()
    program = resolve_launcher(launcher, explicit, host)
    cmd = build_command(launcher, program, host_path(artifact, host), host)

    print(f"=== Launching {launcher} ({sys.platform}) ===")
    if program:
        print(f"  launcher: {program}")
    print(f"  artifact: {cmd[-1]}")

    spawner(cmd)
    return cmd


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        print(__doc__)
        return 1

    try:
        run(argv[0], argv[1] if len(argv) > 1 else None)
    except RunError as exc:
        print(f"run_target: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
