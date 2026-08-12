#!/usr/bin/env python3
"""Vulkan→GL 零拷贝互操作测试驱动脚本。

用法:
    python run_test.py                    # 配置+构建+运行（build 目录默认 build）
    python run_test.py -b <dir>           # 指定 build 目录
    python run_test.py --skip-build       # 只运行已构建的测试

退出码: 0=PASS, 2=SKIP, 1=FAIL。
"""
import argparse
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VCPKG = os.environ.get("VCPKG_ROOT", "C:/vcpkg")


def toolchain():
    tc = os.path.join(VCPKG, "scripts", "buildsystems", "vcpkg.cmake")
    if not os.path.exists(tc):
        print(f"FAIL: 未找到 vcpkg toolchain: {tc}")
        return None
    return tc


def build(build_dir):
    tc = toolchain()
    if tc is None:
        return 1
    cfg = subprocess.run(
        ["cmake", "-S", REPO, "-B", build_dir,
         f"-DCMAKE_TOOLCHAIN_FILE={tc}", "-DVIC_BUILD_TESTS=ON"],
        cwd=REPO)
    if cfg.returncode != 0:
        print("FAIL: CMake 配置失败")
        return 1
    bld = subprocess.run(
        ["cmake", "--build", build_dir, "--config", "Debug",
         "--target", "gl_interop_test"],
        cwd=REPO)
    if bld.returncode != 0:
        print("FAIL: 构建失败")
        return 1
    return 0


def run(build_dir):
    exe = os.path.join(build_dir, "tests", "Debug", "gl_interop_test.exe")
    if not os.path.exists(exe):
        exe = os.path.join(build_dir, "tests", "gl_interop_test.exe")
    if not os.path.exists(exe):
        print(f"FAIL: 测试可执行不存在: {exe}")
        return 1
    r = subprocess.run([exe])
    code = r.returncode
    if code == 0:
        print("PASS")
    elif code == 2:
        print("SKIP: 环境不支持互操作扩展（Vulkan/GL）")
    else:
        print("FAIL")
    return code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-b", "--build-dir", default="build")
    ap.add_argument("--skip-build", action="store_true")
    args = ap.parse_args()
    if not args.skip_build:
        c = build(args.build_dir)
        if c != 0:
            return c
    return run(args.build_dir)


if __name__ == "__main__":
    sys.exit(main())
