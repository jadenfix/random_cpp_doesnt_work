"""Exercise CSV parsing and CLI status with committed historical observations."""

import pathlib
import shutil
import subprocess
import sys
import tempfile


def main():
    """Run the backtester against a historical excerpt and a missing path."""
    binary = pathlib.Path(sys.argv[1]).resolve()
    fixture = pathlib.Path(__file__).parent / "fixtures" / "btc_2024_data.csv"
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        dataset = root / "2024_only"
        dataset.mkdir()
        shutil.copyfile(fixture, dataset / fixture.name)
        success = subprocess.run([str(binary), str(root)], capture_output=True, text=True, timeout=30)
        assert success.returncode == 0, success.stdout + success.stderr
        assert "Parsed 3 rows" in success.stdout, success.stdout + success.stderr
        assert "Finished Strategy: MACrossover_5_20" in success.stdout
        failure = subprocess.run([str(binary), str(root / "missing")],
                                 capture_output=True, text=True, timeout=30)
        assert failure.returncode != 0
        assert "No backtests completed" in failure.stderr
    print("Backtester smoke checks passed: historical CSV replay and missing-data failure")


if __name__ == "__main__":
    main()
