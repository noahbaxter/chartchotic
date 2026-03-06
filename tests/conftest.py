import pytest
import sys
import os
from pathlib import Path

TESTS_DIR = Path(__file__).parent
sys.path.insert(0, str(TESTS_DIR))
PROJECT_ROOT = TESTS_DIR.parent
FIXTURES_DIR = TESTS_DIR / "fixtures"

PLUGIN_NAME = "Chartchotic"


def get_platform_plugin_path():
    env_path = os.environ.get("CHARTCHOTIC_VST3_PATH")
    if env_path:
        return Path(env_path)

    if sys.platform == "darwin":
        return Path.home() / f"Library/Audio/Plug-Ins/VST3/{PLUGIN_NAME}.vst3"
    elif sys.platform == "win32":
        return Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / f"Common Files/VST3/{PLUGIN_NAME}.vst3"
    else:
        return Path.home() / f".vst3/{PLUGIN_NAME}.vst3"


@pytest.fixture
def plugin_path():
    path = get_platform_plugin_path()
    if not path.exists():
        pytest.skip(f"Plugin not found at {path}. Run ./build.sh first.")
    return str(path)


@pytest.fixture
def unit_tests_binary():
    if sys.platform == "win32":
        path = TESTS_DIR / "unit/build/Release/unit_tests.exe"
    else:
        path = TESTS_DIR / "unit/build/unit_tests_artefacts/Release/unit_tests"
    if not path.exists():
        pytest.skip(f"Unit tests binary not found at {path}. Build with cmake first.")
    return str(path)


@pytest.fixture
def pluginval_path():
    import shutil

    pluginval = shutil.which("pluginval")
    if pluginval:
        return pluginval

    if sys.platform == "darwin":
        paths = [
            "/Applications/pluginval.app/Contents/MacOS/pluginval",
            "/usr/local/bin/pluginval",
            Path.home() / "bin/pluginval",
        ]
    elif sys.platform == "win32":
        paths = [
            Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / "pluginval/pluginval.exe",
            Path.home() / "bin/pluginval.exe",
        ]
    else:
        paths = [
            "/usr/local/bin/pluginval",
            Path.home() / "bin/pluginval",
        ]

    for p in paths:
        if Path(p).exists():
            return str(p)
    pytest.skip("pluginval not found. Install from https://github.com/Tracktion/pluginval")


@pytest.fixture
def fixtures_dir():
    return FIXTURES_DIR
