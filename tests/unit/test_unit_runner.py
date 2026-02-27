import subprocess


def test_cpp_unit_tests(unit_tests_binary):
    result = subprocess.run(
        [unit_tests_binary],
        capture_output=True,
        text=True
    )

    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr)

    assert result.returncode == 0, f"C++ unit tests failed:\n{result.stderr}"
