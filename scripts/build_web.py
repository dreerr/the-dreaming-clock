"""PlatformIO pre-hook: rebuild the web frontend before the firmware builds.

The output in data/ is committed, so this is an optimisation, not a
requirement — if Node is missing the build continues with whatever is already
in data/. That keeps `pio run -t uploadfs` working on a clean checkout with no
JS toolchain installed.
"""

import os
import shutil
import subprocess

Import("env")  # noqa: F821  (injected by PlatformIO)

project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
web_dir = os.path.join(project_dir, "web")
build_script = os.path.join(web_dir, "build.mjs")

if not os.path.isfile(build_script):
    print("build_web: web/build.mjs not found, skipping")
elif shutil.which("node") is None:
    print("build_web: node not found, using the committed data/ as-is")
elif not os.path.isdir(os.path.join(web_dir, "node_modules")):
    print("build_web: run 'npm install' in web/ to rebuild the frontend; "
          "using the committed data/ as-is")
else:
    print("build_web: bundling web/ -> data/")
    result = subprocess.run(["node", build_script], cwd=web_dir)
    if result.returncode != 0:
        raise SystemExit("build_web: frontend build failed")
