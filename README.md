/Users/jadenfix/algorithmic_trading_bots/
├── .git/                   # Hidden directory containing Git repository data
├── .gitignore              # Specifies intentionally untracked files (build/, *.o, etc.)
├── .vscode/                # (This should be in .gitignore but wasn't initially) - VS Code settings
├── CMakeLists.txt          # Instructions for CMake build system
├── data/                   # Directory containing input CSV data
│   ├── quant_seconds_data_MSFT.csv
│   ├── quant_seconds_data_NVDA.csv
│   └── quant_seconds_data_google.csv
├── external/
│   └── csv2                # <--- Currently a GITLINK (Mode 160000) referencing the csv2 repo commit
│                           #      Does NOT contain the actual csv2 code files in *your* repo yet.
└── src/                    # Directory containing C++ source code
    ├── DataManager.cpp     # Implementation of the DataManager class
    ├── DataManager.h       # Header declaration for the DataManager class
    ├── PriceBar.h          # Header definition for the PriceBar struct
    └── main.cpp            # Main executable entry point and demonstration logic
