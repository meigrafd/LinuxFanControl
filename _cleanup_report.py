#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Repo cleanup report for LinuxFanControl.
- Finds files not referenced by CMake or source includes.
- Skips build/.git folders automatically.
- Writes cleanup_candidates.txt
"""
import os, re, sys, json
ROOT = os.path.abspath(os.path.dirname(__file__) + "/..") if os.path.basename(os.getcwd())=="tools" else os.getcwd()

# Configuration
SRC_EXTS  = {".cpp",".cc",".c",".cxx",".hpp",".hh",".h",".hxx",".ui",".qrc",".qml",".qss",".svg",".png",".ico"}
KEEP_DIRS = {"src","assets","scripts",".github"}  # consider under ROOT
SKIP_DIRS = {".git","build",".idea",".vscode","cmake-build","out","dist","__pycache__"}

# 1) Collect all files
all_files=[]
for dp, dn, fn in os.walk(ROOT):
    # skip dirs
    if any(part in SKIP_DIRS for part in dp.replace("\\","/").split("/")):
        continue
    for f in fn:
        p = os.path.join(dp,f)
        rel = os.path.relpath(p, ROOT)
        all_files.append(rel)

# 2) Parse CMake files
cmake_files=[p for p in all_files if os.path.basename(p).lower()=="cmakelists.txt" or p.lower().endswith(".cmake")]
cmake_refs=set()
cm_pat = re.compile(r'(?i)(add_executable|add_library|target_sources|set|qt_add_executable)\s*\([^)]+\)', re.S)

def norm(path):
    return os.path.normpath(path).replace("\\","/")

for cf in cmake_files:
    try:
        txt=open(os.path.join(ROOT,cf),"r",encoding="utf-8",errors="ignore").read()
    except: continue
    # naive capture of quoted paths
    for m in cm_pat.finditer(txt):
        block=m.group(0)
        for q in re.findall(r'["\']([^"\']+)["\']', block):
            if any(q.strip().endswith(ext) for ext in SRC_EXTS):
                cmake_refs.add(norm(os.path.relpath(os.path.join(os.path.dirname(os.path.join(ROOT,cf)), q), ROOT)))
        # bare words with extensions
        for w in re.findall(r'[\s(]([^\s)]+)', block):
            if any(w.strip().endswith(ext) for ext in SRC_EXTS):
                cmake_refs.add(norm(os.path.relpath(os.path.join(os.path.dirname(os.path.join(ROOT,cf)), w), ROOT)))

# 3) Parse source includes / resource uses
src_like = [p for p in all_files if os.path.splitext(p)[1] in {".cpp",".cc",".cxx",".hpp",".hh",".h",".qml",".ui",".qrc",".py",".qss"}]
inc_refs=set()
inc_pat = re.compile(r'#\s*include\s*["<]([^">]+)[">]')
qss_pat = re.compile(r'assets/themes/(light|dark)\.qss')
res_pat = re.compile(r'assets/[^"\']+\.(svg|png|ico|qss)')

for sf in src_like:
    try:
        txt=open(os.path.join(ROOT,sf),"r",encoding="utf-8",errors="ignore").read()
    except: continue
    # includes (relative)
    for inc in inc_pat.findall(txt):
        inc_rel = norm(os.path.relpath(os.path.join(os.path.dirname(os.path.join(ROOT,sf)), inc), ROOT))
        inc_refs.add(inc_rel)
    # assets used
    for m in res_pat.findall(txt):
        pass
    for m in re.findall(r'assets/[^\s"\'\)]+', txt):
        inc_refs.add(norm(m))

# 4) Build keep set
keep=set()
keep |= cmake_refs
keep |= inc_refs
# Also keep top-level files
for f in all_files:
    base = f.split("/")[0]
    if base in KEEP_DIRS:
        continue
    # keep root metadata
    if "/" not in f and os.path.splitext(f)[1] in {".md",".txt",".sh",".cmake",".yml",".yaml",".json",".toml",".ini",".gitignore"}:
        keep.add(f)

# 5) Candidates: only real files, not referenced, under known trees, excluding build/ .git etc.
candidates=[]
for f in all_files:
    if any(part in SKIP_DIRS for part in f.split("/")):
        continue
    ext = os.path.splitext(f)[1].lower()
    if ext in SRC_EXTS or ext in {".py",".patch",".log"}:
        if f not in keep:
            candidates.append(f)

# 6) Heuristics: drop Python protos, patch files first
pri=lambda p: (0 if p.endswith(".patch") else 1 if p.endswith(".py") else 2, p.lower())

candidates.sort(key=pri)
out=os.path.join(ROOT,"cleanup_candidates.txt")
with open(out,"w",encoding="utf-8") as f:
    for c in candidates: f.write(c+"\n")

print("[i] Wrote", out)
print("[i] Candidates:", len(candidates))
print("---- preview (first 40) ----")
for c in candidates[:40]: print("  ", c)
print("---- git dry-run (copy/paste to test) ----")
if candidates:
    # protect spaces
    quoted = " ".join("'" + c.replace("'", "'\\''") + "'" for c in candidates)
    print(f"git rm -n {quoted}")
else:
    print("nothing to remove")
