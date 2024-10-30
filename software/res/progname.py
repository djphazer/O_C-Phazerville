Import("env")

import subprocess

def get_git_rev():
    git_rev = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
    git_status = subprocess.check_output(["git", "status", "-s", "--untracked-files=no"]).decode()
    suffix = "dirty" if git_status else ""

    return git_rev + suffix

def get_version():
    with open("src/OC_version.h") as file:
        last_line = file.readlines()[-1].strip().replace('"', '')

    return last_line 


version = get_version()
git_rev = get_git_rev()
extras = ""
env.Append(BUILD_FLAGS=[ f'-DOC_BUILD_TAG=\\"{git_rev}\\"' ])

build_flags = env.ParseFlags(env['BUILD_FLAGS'])
defines = build_flags.get("CPPDEFINES")
for item in defines:
    if item[0] == 'OC_VERSION_EXTRA':
        extras += item[1].strip('"')

if "VOR" in defines:
    extras += "+VOR"
if "FLIP_180" in defines:
    extras += "_flipped"

env.Replace(PROGNAME=f"o_C-phazerville-{version}{extras}-{git_rev}")
