# PE inspector

Install `pefile` for Python 3.12 or newer, then run:

```powershell
python -I tools/inspect_pe.py path/to/image.exe
```

The tool reads the file and prints its SHA-256, architecture, entry point, sections, import-library counts and unwind-function count as JSON. It does not execute or modify the input. Generated reports belong outside the repository because they can contain information from the user's executable.
