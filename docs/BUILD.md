# Build and run the backtester

The executable runs the traditional strategies defined in `src/main.cpp`.
The DRL files remain a prototype: their base inference interface and model
integration are incomplete, and they are not included in this target.

```bash
cmake -S . -B build
cmake --build build --parallel
./build/FinancialBacktester ./data
ctest --test-dir build --output-on-failure
```

CMake fetches csv2 at the revision already recorded by the repository's old
gitlink. For an offline build, provide a checkout of that revision:

```bash
cmake -S . -B build -DCSV2_INCLUDE_DIR=/path/to/csv2/include
```

The optional first argument is the root containing `stocks_april`,
`2024_only`, and `2024_2025`. Without an argument it defaults to `data`
relative to the current directory. A run that produces no backtest results
exits with status 1, so missing or unreadable datasets cannot pass a smoke
check.

The CSV reader accepts the comma delimiter and ISO dates used by the checked-in
crypto data, as well as the M/D/YY dates used by older exports. The smoke test
replays the first three real BTC rows from `data/2024_only/btc_2024_data.csv`
and verifies that an absent dataset returns failure. It validates the pipeline,
not strategy profitability or the incomplete DRL prototype.

Each CSV now supplies its own column mapping. Files in the same dataset may
reorder columns or include additional numeric fields; price and timestamp
columns are read from that file's header. Empty or duplicate column names and
missing required date/time/close columns reject the load. The additional CTest
case checks schema and input-integrity scenarios through the production reader
and the backtester executable. Numeric fields must contain a complete finite
number; OHLC prices must be positive and volume nonnegative. Signed auxiliary
numeric fields and zero volume are allowed. Empty/header-only files, malformed
row widths, unparseable timestamps and invalid numbers reject the entire load
and clear its partial event stream. Blank trailing lines are ignored. Invalid
rows are never converted to zero or silently excluded from a successful replay.

These checks do not validate strategy returns, calendar normalization/timezone
semantics, OHLC cross-field consistency, or the DRL prototype.
