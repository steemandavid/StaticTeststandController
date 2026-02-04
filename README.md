# Static Teststand Controller

Controller application for static teststand operations.

## Requirements

- Python 3.11 or higher

## Installation

1. Create and activate a virtual environment:
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   ```

2. Install the package:
   ```bash
   pip install -e .
   ```

   For development:
   ```bash
   pip install -e ".[dev]"
   ```

## Project Structure

```
StaticTeststandController/
├── src/
│   └── static_teststand_controller/   # Main package
│       └── __init__.py
├── tests/                             # Test suite
├── config/                            # Configuration files
├── docs/                              # Documentation
├── pyproject.toml                     # Project configuration
└── README.md
```

## Development

Run tests:
```bash
pytest
```

Type checking:
```bash
mypy src/
```

Linting:
```bash
ruff check src/
```
