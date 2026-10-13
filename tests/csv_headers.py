"""Check per-file schema handling through the actual CSV reader and executable."""

import csv
import io
import pathlib
import subprocess
import sys
import tempfile
import unittest

PROBE = pathlib.Path(sys.argv.pop(1)).resolve()
BACKTESTER = pathlib.Path(sys.argv.pop(1)).resolve()
FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "btc_2024_data.csv"


class CsvHeaderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        self.dataset = self.root / "2024_only"
        self.dataset.mkdir()
        self.rows = list(csv.DictReader(io.StringIO(FIXTURE.read_text())))
        self.headers = list(self.rows[0])
        self.write_file("btc_2024_data", self.headers)

    def write_file(self, symbol, headers):
        with (self.dataset / (symbol + ".csv")).open("w", newline="") as stream:
            writer = csv.writer(stream, lineterminator="\n")
            writer.writerow(headers)
            writer.writerows([[row.get(key, "1.0") for key in headers] for row in self.rows])

    def probe(self):
        return subprocess.run([str(PROBE), str(self.dataset)], capture_output=True,
                              text=True, timeout=30)

    def check_prices(self, headers):
        self.write_file("eth_2024_data", headers)
        result = self.probe()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        bars = [line.split("\t") for line in result.stdout.splitlines()
                if line.startswith("BAR\t")]
        self.assertEqual(len(bars), 6, result.stdout)
        for symbol in ("btc_2024_data", "eth_2024_data"):
            values = [(float(bar[2]), float(bar[3])) for bar in bars if bar[1] == symbol]
            self.assertEqual(values, [(float(row["close"]), float(row["volume"]))
                                      for row in self.rows])
        run = subprocess.run([str(BACKTESTER), str(self.root)], capture_output=True,
                             text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn("Total loaded: 6,", run.stdout)
        self.assertIn("Finished Strategy: MACrossover_5_20", run.stdout)

    def test_same_headers(self):
        self.check_prices(self.headers)

    def test_reordered_price_columns(self):
        headers = self.headers.copy()
        a, b = headers.index("close"), headers.index("volume")
        headers[a], headers[b] = headers[b], headers[a]
        self.check_prices(headers)

    def test_reordered_timestamp_columns(self):
        self.check_prices(list(reversed(self.headers)))

    def test_additional_numeric_column(self):
        self.check_prices(self.headers + ["extra_measurement"])

    def test_duplicate_column_rejected(self):
        self.write_file("eth_2024_data", self.headers + ["close"])
        result = self.probe()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("duplicate column", result.stderr)

    def test_missing_required_column_rejected(self):
        self.write_file("eth_2024_data", [h for h in self.headers if h != "close"])
        result = self.probe()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("missing required", result.stderr)


if __name__ == "__main__":
    unittest.main()
