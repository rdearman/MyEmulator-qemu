"""Scope temporary kernel test configuration to its fixture build."""

from contextlib import contextmanager
import os


@contextmanager
def temporary_linux_config(path):
    path = os.fspath(path)
    with open(path, "rb") as config:
        previous = config.read()
    previous_allow = os.environ.get("MYEMU_ALLOW_TEST_INITRAMFS")
    os.environ["MYEMU_ALLOW_TEST_INITRAMFS"] = "1"
    try:
        yield
    finally:
        with open(path, "wb") as config:
            config.write(previous)
        if previous_allow is None:
            os.environ.pop("MYEMU_ALLOW_TEST_INITRAMFS", None)
        else:
            os.environ["MYEMU_ALLOW_TEST_INITRAMFS"] = previous_allow
