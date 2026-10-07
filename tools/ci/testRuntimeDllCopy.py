# Copyright (C) 2026 s25coop contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Exercise real generated post-build scripts, including an executed lock-omission control."""

import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def run(command):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result.stdout


def contend(cmake, folder, destination, scripts, payload):
    ready, release = folder / "lock-ready", folder / "lock-release"
    ready.unlink(missing_ok=True)
    release.unlink(missing_ok=True)
    output = destination / "runtime library.dll"
    output.unlink(missing_ok=True)
    holder_script = folder / "hold-lock.cmake"
    holder_script.write_text(f'''file(LOCK "{destination.as_posix()}/.rttr-runtime-dlls.lock" GUARD PROCESS TIMEOUT 10)
file(WRITE "{ready.as_posix()}" "ready")
foreach(i RANGE 1 200)
    if(EXISTS "{release.as_posix()}")
        return()
    endif()
    execute_process(COMMAND "{Path(cmake).as_posix()}" -E sleep 0.1)
endforeach()
message(FATAL_ERROR "Lock holder timed out")
''')
    processes = []
    holder = subprocess.Popen([cmake, "-P", str(holder_script)], stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True)
    try:
        deadline = time.monotonic() + 10
        while not ready.exists():
            if holder.poll() is not None or time.monotonic() > deadline:
                raise RuntimeError("Lock holder did not acquire lock: " + holder.communicate()[0])
            time.sleep(0.05)
        for script in scripts:
            processes.append(subprocess.Popen([cmake, "-P", str(script)], stdout=subprocess.PIPE,
                                              stderr=subprocess.STDOUT, text=True))
        time.sleep(1)
        blocked = all(p.poll() is None for p in processes) and not output.exists()
        release.touch()
        for p in processes:
            log = p.communicate(timeout=20)[0]
            if p.returncode:
                raise RuntimeError(log)
        log = holder.communicate(timeout=20)[0]
        if holder.returncode:
            raise RuntimeError(log)
        assert hashlib.sha256(output.read_bytes()).digest() == hashlib.sha256(payload).digest()
        return blocked
    finally:
        release.touch()
        for p in [*processes, holder]:
            if p.poll() is None:
                p.kill()
            p.communicate()


def main():
    cmake = shutil.which("cmake")
    assert cmake, "CMake must be installed"
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="s25-runtime-dlls-") as tmp:
        folder = Path(tmp)
        source, build = folder / "source with spaces", folder / "build with spaces"
        source.mkdir()
        payload = bytes(range(256)) * 4096
        (source / "runtime library.dll").write_bytes(payload)
        (source / "main.c").write_text("int main(void) { return 0; }\n")
        # Linux also exercises the Windows-only generator path; no product compiler/platform override.
        (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(RuntimeDllCopyTest C)
set(WIN32 TRUE)
set(GATHER_DLLS "${{CMAKE_CURRENT_SOURCE_DIR}}/runtime library.dll")
include("{root.as_posix()}/cmake/Modules/GatherDll.cmake")
foreach(target first second)
    add_executable(${{target}} main.c)
    set_target_properties(${{target}} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/output with spaces")
    gather_dll_copy(${{target}})
endforeach()
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/destination-$<CONFIG>.txt" CONTENT "$<TARGET_FILE_DIR:first>")
''')
        run([cmake, "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug"])
        run([cmake, "--build", str(build), "--config", "Debug", "--parallel", "2"])
        destination = Path((build / "destination-Debug.txt").read_text())
        assert (destination / "runtime library.dll").read_bytes() == payload
        scripts = [build / f"copy-runtime-dlls-{target}-Debug.cmake" for target in ["first", "second"]]
        assert contend(cmake, folder, destination, scripts, payload), "DLL copy ignored destination lock"
        negatives = []
        for script in scripts:
            text = script.read_text()
            start = text.index("file(LOCK ")
            end = text.index("foreach(source", start)
            negative = script.with_suffix(".negative.cmake")
            negative.write_text(text[:start] + text[end:])
            negatives.append(negative)
        # One unprotected writer proves bypass without recreating the nondeterministic Windows race.
        assert not contend(cmake, folder, destination, negatives[:1], payload), "Omission control did not bypass lock"
        print("Parallel post-build DLL copies wait for shared destination lock; executed omission bypasses it.")


if __name__ == "__main__":
    main()
