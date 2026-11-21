#!/usr/bin/env python3
"""Package build artefacts exactly as the GitHub Release does. Output: dist/"""
import argparse, hashlib, pathlib, shutil, subprocess, tarfile, zipfile

NAME = "Memoria by DSK"
SLUG = "Memoria-by-DSK"

def zip_tree(src: pathlib.Path, out: pathlib.Path):
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        if src.is_file():
            z.write(src, src.name)
        else:
            for f in sorted(src.rglob("*")):
                z.write(f, pathlib.Path(src.name) / f.relative_to(src))

def tar_tree(src: pathlib.Path, out: pathlib.Path):
    with tarfile.open(out, "w:gz") as t:
        t.add(src, arcname=src.name)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--platform", choices=["win", "mac", "linux"], required=True)
    ap.add_argument("--build", default="build")
    ap.add_argument("--config", default="Release")
    ap.add_argument("--out", default="dist")
    ap.add_argument("--checksums-only", action="store_true")
    ap.add_argument("--version", default="0.0.0")
    a = ap.parse_args()
    art = pathlib.Path(a.build) / "Memoria_artefacts" / a.config
    out = pathlib.Path(a.out); out.mkdir(exist_ok=True)
    vst3 = art / "VST3" / f"{NAME}.vst3"
    made = []
    if a.checksums_only:
        pass
    elif a.platform == "win":
        exe = art / "Standalone" / f"{NAME}.exe"
        made.append(out / f"{SLUG}-win-x64-vst3.zip"); zip_tree(vst3, made[-1])
        made.append(out / f"{SLUG}-win-x64-standalone.zip"); zip_tree(exe, made[-1])
    elif a.platform == "mac":
        au = art / "AU" / f"{NAME}.component"
        app = art / "Standalone" / f"{NAME}.app"
        for b in (vst3, au, app):  # ad-hoc sign (no Apple developer identity available)
            subprocess.run(["codesign", "--force", "--deep", "-s", "-", str(b)], check=True)
        for b, kind in ((vst3, "vst3"), (au, "au"), (app, "standalone")):
            made.append(out / f"{SLUG}-macos-universal-{kind}.zip")
            subprocess.run(["ditto", "-c", "-k", "--keepParent", str(b), str(made[-1])], check=True)
        root = pathlib.Path(a.build) / "pkgroot"
        shutil.rmtree(root, ignore_errors=True)
        (root / "Library/Audio/Plug-Ins/VST3").mkdir(parents=True)
        (root / "Library/Audio/Plug-Ins/Components").mkdir(parents=True)
        (root / "Applications").mkdir(parents=True)
        subprocess.run(["ditto", str(vst3), str(root / "Library/Audio/Plug-Ins/VST3" / vst3.name)], check=True)
        subprocess.run(["ditto", str(au), str(root / "Library/Audio/Plug-Ins/Components" / au.name)], check=True)
        subprocess.run(["ditto", str(app), str(root / "Applications" / app.name)], check=True)
        made.append(out / f"{SLUG}-macos-universal-installer.pkg")
        subprocess.run(["pkgbuild", "--root", str(root), "--identifier", "com.dskmusic.memoria.pkg",
                        "--version", a.version, "--install-location", "/", str(made[-1])], check=True)
    else:
        exe = art / "Standalone" / NAME
        made.append(out / f"{SLUG}-linux-x64-vst3.tar.gz"); tar_tree(vst3, made[-1])
        made.append(out / f"{SLUG}-linux-x64-standalone.tar.gz"); tar_tree(exe, made[-1])
    with open(out / f"SHA256SUMS-{a.platform}.txt", "w", newline="\n") as s:
        for f in sorted(out.iterdir()):
            if f.suffix in (".zip", ".gz", ".exe", ".pkg"):
                s.write(f"{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n")
    for f in made: print("packaged", f)

if __name__ == "__main__":
    main()
