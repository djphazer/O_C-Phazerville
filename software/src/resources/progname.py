Import("env")

import subprocess

version = subprocess.check_output("tail -n1 'OC_version.h'|tr -d '\"'", shell=True).decode().strip()
# git_rev = subprocess.check_output("git rev-parse --short HEAD", shell=True).decode().strip()
git_rev = subprocess.check_output("sh resources/oc_build_tag.sh", shell=True).decode().strip()
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
