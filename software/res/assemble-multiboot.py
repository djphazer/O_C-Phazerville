Import("env")

import subprocess
from os.path import join

def get_git_rev():
    git_rev = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
    git_status = subprocess.check_output(["git", "status", "-s", "--untracked-files=no"]).decode()
    suffix = "dirty" if git_status else ""

    return git_rev + suffix

def get_version():
    with open("src/OC_version.h") as file:
        last_line = file.readlines()[-1].strip().replace('"', '')

    return last_line 

git_rev = get_git_rev()
env.Append(BUILD_FLAGS=[ f'-DOC_BUILD_TAG=\\"{git_rev}\\"' ])

def after_build(source, target, env):
    git_rev = get_git_rev()
    version = get_version()
    build_flags = env.ParseFlags(env['BUILD_FLAGS'])
    defines = build_flags.get("CPPDEFINES")
    for item in defines:
        if item[0] == 'OC_VERSION_EXTRA':
            version += item[1].strip('"')
    env.Replace(PROGNAME=f"o_C-phazerville-{version}-{git_rev}")

    app_A = env.subst(".pio/build/T41/firmware.hex")
    app_B = env.subst(".pio/build/T41_audio/firmware.hex")
    app_X = env.subst(".pio/build/T41_MTP/firmware.hex")
    # app_Y = env.subst("")

    out = env.subst("${PROGNAME}.hex")

    platform = env.PioPlatform()
    subprocess.call([join(platform.get_package_dir("tool-sreccat") or "", "srec_cat"), app_A, "-Intel", app_B, "-Intel", app_X, "-Intel", "-o", out, "-Intel"])

env.AddPostAction("buildprog", after_build)
