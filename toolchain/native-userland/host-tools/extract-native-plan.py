#!/usr/bin/env python3
"""Extract source-only native Samurai recipes from an upstream configured build."""
from pathlib import Path
import shlex
import re
import os
import subprocess
import sys

workspace = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
tools = Path(__file__).resolve().parent
configuration_deps = str(workspace / "host-build-plans/native-deps")
package = workspace / "kit/packages" / sys.argv[1]
configured = Path(sys.argv[2]).resolve()
targets = sys.argv[3:]
if not targets:
    raise SystemExit("Provide upstream binary targets")
recipes = []
expected = []
changed_sources = sorted(set(package.rglob("*.c")) | set(configured.rglob("*.c")))
force_sources = ["-W" + str(path) for path in changed_sources]
for target in targets:
    directory, separator, name = target.partition(":")
    if not separator:
        directory, name = ".", target
    assumed_archives = ["-W" + os.path.relpath(configured / path, configured / directory)
                        for path in expected if path.endswith((".a", ".la"))]
    for path in expected:
        if path.endswith((".a", ".la")):
            prefix = "../" * len(Path(directory).parts) if directory != "." else ""
            assumed_archives.append("-W" + prefix + path)
    source_search = (["srcdir=" + str(package / "src"),
                      "VPATH=" + str(package / "src"),
                      "-f", "Makefile", "-f", "native-extract.mk"]
                     if package.name.startswith("vim-") else [])
    run = subprocess.run(["make", "-n", *source_search,
                          "CC=" + str(tools / "chibicc-configure-cc"),
                          *force_sources, *assumed_archives, name],
                         cwd=configured / directory,
                         capture_output=True, text=True)
    if run.returncode:
        sys.exit(run.stderr)
    recipes.append((directory, run.stdout))
    expected.append(os.path.normpath(directory + "/" + name))
text = "\n".join(output for directory, output in recipes)
(configured / "native-recipes.log").write_text(text)
compiler = str(tools / "chibicc-configure-cc")
compiles = {}
archives = {}
links = {}
for directory, output in recipes:
  for line in output.splitlines():
    if compiler in line:
        line = line[line.index(compiler):].rstrip("\\").strip()
        def source_path(match):
            relative, prefix, suffix = match.groups()
            return relative if (configured / directory / relative).exists() else prefix + suffix
        line = re.sub(r"`test -f '([^']+)' \|\| echo '([^']+)'`(\S+)",
                      source_path, line)
        if line.endswith("&&"):
            line = line[:-2].rstrip()
        args = shlex.split(line)[1:]
        if "-o" not in args:
            raise SystemExit(f"No output in compiler recipe: {line}")
        out = args[args.index("-o") + 1]
        out = os.path.normpath(directory + "/" + out)
        if out.endswith(".lo"):
            out = out[:-3] + ".o"
        if "-c" in args:
            source = next((a for a in reversed(args) if a.endswith(".c")), None)
            if source is None:
                raise SystemExit(f"No C source in recipe: {line}")
            flags = []
            i = 0
            while i < len(args):
                arg = args[i]
                if arg in ("-o", "-MT", "-MF", "-MQ"):
                    i += 2
                    continue
                if arg in ("-c", "-MD", "-MMD", "-MP") or arg == source:
                    i += 1
                    continue
                if arg.startswith("-W"):
                    i += 1
                    continue
                if arg in ("-O1", "-O2", "-O3", "-Os", "-funroll-loops",
                           "-fomit-frame-pointer", "-fno-strict-aliasing"):
                    i += 1
                    continue
                if arg.startswith("-I"):
                    include = arg[2:]
                    if not Path(include).is_absolute():
                        include = os.path.normpath(directory + "/" + include)
                        if not (configured / include).exists() and (package / "src" / include).is_dir():
                            include = str(package / "src" / include)
                    flags.append("-I" + include.replace(str(package), "..")
                                 .replace(configuration_deps, "@REM_DEPS@"))
                elif arg.startswith("-D") or arg in ("-O0",):
                    flags.append(arg)
                else:
                    raise SystemExit(f"Unclassified compile flag: {arg}")
                i += 1
            if not Path(source).is_absolute():
                source = os.path.normpath(directory + "/" + source)
                if not (configured / source).exists() and (package / "src" / source).is_file():
                    source = str(package / "src" / source)
            compiles[out] = (source.replace(str(package), ".."), flags)
        else:
            inputs = []
            for index, arg in enumerate(args):
                if index == args.index("-o") + 1:
                    continue
                if not arg.endswith((".o", ".a", ".lo", ".la")):
                    continue
                arg = os.path.normpath(directory + "/" + arg)
                if arg.endswith(".lo"):
                    arg = arg[:-3] + ".o"
                elif arg.endswith(".la"):
                    arg = arg[:-3] + ".a"
                inputs.append(arg)
            libraries = []
            for arg in args:
                if arg.startswith("-L"):
                    path = arg[2:]
                    if not Path(path).is_absolute():
                        path = os.path.normpath(directory + "/" + path)
                    path = path.replace(configuration_deps, "@REM_DEPS@")
                    libraries.append("-L" + path)
                elif arg.startswith("-l"):
                    libraries.append(arg)
            if out.endswith(".la"):
                archives[out[:-3] + ".a"] = inputs
            else:
                links[out] = (inputs, libraries)
    elif ";ar " in line or line.startswith("ar "):
        recipe = line.split(";")[-1]
        args = shlex.split(recipe)
        if len(args) < 3 or not args[2].endswith(".a"):
            raise SystemExit(f"Unclassified archive command: {line}")
        archives[os.path.normpath(directory + "/" + args[2])] = [
            os.path.normpath(directory + "/" + arg) for arg in args[3:]]

expected_outputs = {path[:-3] + ".a" if path.endswith(".la") else path
                    for path in expected}
missing = expected_outputs - set(links) - set(archives)
if missing:
    raise SystemExit(f"Missing native link recipes: {sorted(missing)}")
for out, (inputs, libraries) in links.items():
    directories = [flag[2:] for flag in libraries if flag.startswith("-L")]
    unresolved = []
    for flag in libraries:
        matches = [path for path in archives
                   if flag.startswith("-l") and
                   Path(path).name == "lib" + flag[2:] + ".a" and
                   str(Path(path).parent) in directories]
        if len(matches) > 1:
            raise SystemExit(f"Ambiguous local library {flag}: {matches}")
        if matches:
            inputs.append(matches[0])
        else:
            unresolved.append(flag)
    links[out] = inputs, unresolved
missing_objects = {path for inputs in archives.values() for path in inputs
                   if path not in compiles}
missing_objects |= {path for inputs, libraries in links.values() for path in inputs
                    if path not in compiles and path not in archives}
if missing_objects:
    raise SystemExit(f"Missing native object recipes: {sorted(missing_objects)}")
destination = package / "rem-native-plan"
destination.mkdir(exist_ok=True)
generated = []
for path in configured.rglob("*"):
    if path.is_file() and path.suffix in (".h", ".c"):
        relative = path.relative_to(configured)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        contents = path.read_text()
        contents = contents.replace("/home/dev/.local", "@REM_PREFIX@")
        contents = contents.replace(configuration_deps, "@REM_DEPS@")
        target.write_text(contents)
        generated.append(str(relative))
(destination / "source-files.list").write_text("\n".join(sorted(generated)) + "\n")

def quote(value):
    return shlex.quote(value).replace("$", "$$")

def paths(values):
    return " ".join(v.replace("$", "$$").replace(" ", "$ ").replace(":", "$:")
                    for v in values)

ninja = [
    "cc = @CC@", "root = ../../..", "",
    "rule cc",
    "  command = mkdir -p $directory && $cc $flags -c $in -o $out",
    "  description = CC $in",
    "rule archive",
    "  command = ar rcs $out $in",
    "  description = AR $out",
    "rule link",
    "  command = mkdir -p ../native/bin && ld -r -T $root/bootstrap/rem-reloc.ld -o $out.partial.o $in && $cc -o $out $out.partial.o $libraries",
    "  description = LINK $out", "",
]
for out, (source, flags) in compiles.items():
    headers = [name for name in generated if name.endswith(".h")]
    ninja += [f"build {paths([out])}: cc {paths([source])} | {paths(headers)}",
              f"  directory = {quote(str(Path(out).parent))}",
              "  flags = " + " ".join(quote(flag) for flag in flags)]
for out, inputs in archives.items():
    ninja.append(f"build {paths([out])}: archive {paths(list(dict.fromkeys(inputs)))}")
defaults = list(archives)
for out, (inputs, libraries) in links.items():
    name = Path(out).name
    if package.name.startswith("coreutils-") and name == "ginstall":
        name = "install"
    final = "../native/bin/" + name
    ninja += [f"build {paths([final])}: link {paths(list(dict.fromkeys(inputs)))}",
              "  libraries = " + " ".join(quote(lib) for lib in libraries)]
    defaults.append(final)
ninja += ["default " + paths(defaults), ""]
(destination / "build.ninja.in").write_text("\n".join(ninja))
(destination / "PROVENANCE.txt").write_text(
    "Extracted from the upstream Makefile after REM-target cross configuration.\n"
    "Configure compile/link checks used the repaired REM chibicc source backend\n"
    "and the existing REM musl headers. No target binaries are supplied here.\n"
    "Native build compilation, linking and runtime acceptance remain required.\n"
    f"Compile recipes: {len(compiles)}; archives: {len(archives)}; binaries: {len(links)}.\n"
)
print(f"{package.name}: {len(compiles)} compilations, {len(archives)} archives, "
      f"{len(links)} native binaries")
