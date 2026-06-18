# Regression corpus

Store minimized, durable failure records here after bugs are fixed.

The first-stage runner can emit a JSON repro record with:

```powershell
.\x64\Release\test.exe --record-failure failure.json
```

When a failure is fixed, copy the important repro information into this directory and add an explicit test or replay support before relying on it as a permanent guard.
