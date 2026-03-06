# Integration tests for Chartchotic.
# Note: pedalboard can't load this plugin (no audio output), so we verify
# the VST3 bundle structure directly.

from pathlib import Path


def test_vst3_bundle_exists(plugin_path):
    bundle = Path(plugin_path)
    assert bundle.exists()
    assert bundle.suffix == ".vst3"


def test_vst3_bundle_has_binary(plugin_path):
    bundle = Path(plugin_path)
    binary = bundle / "Contents" / "MacOS" / "Chartchotic"
    assert binary.exists(), f"Missing binary at {binary}"
