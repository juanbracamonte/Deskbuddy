Import("env")

import shutil
from pathlib import Path

src = Path(env["PROJECT_DIR"]) / "User_Setup.h"
libdeps = Path(env["PROJECT_LIBDEPS_DIR"]) / env["PIOENV"]

if not src.exists():
    print("User_Setup.h not found in project root")
else:
    targets = list(libdeps.glob("**/TFT_eSPI/User_Setup.h"))
    if not targets:
        print("TFT_eSPI not installed yet; setup copy skipped this pass")
    for dest in targets:
        shutil.copyfile(src, dest)
        print("Copied User_Setup.h -> %s" % dest)
