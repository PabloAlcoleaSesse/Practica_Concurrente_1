"""Check routing at the broker as well as callbacks: local filtering cannot hide errors."""
import subprocess
from pathlib import Path

BUILD_DIR = Path(__file__).resolve().parents[1] / "build"

for attempt in range(5):
    result = subprocess.run(["./broker"], cwd=BUILD_DIR, capture_output=True, text=True, timeout=20)
    assert result.returncode == 0, result.stdout + result.stderr
    output = result.stdout
    assert output.count("[Master] Lanzado P") == 3, output
    for role in (1, 2, 3):
        assert f"[P{role}] OK" in output, output
    for line in (
        "MSG estado INT 42 -> 2 suscriptor(es)",
        "MSG temperatura FLOAT 23.5 -> 2 suscriptor(es)",
        "MSG temperatura FLOAT 24.5 -> 1 suscriptor(es)",
        "MSG alarma STRING Hola mundo desde P3 -> 2 suscriptor(es)",
        "MSG fin INT 1 -> 3 suscriptor(es)",
    ):
        assert line in output, output
    assert "[P3] temperatura = 24.5" not in output, output
print("Integración: OK (5 ejecuciones, 3 hijos, tipos, fan-out, autoentrega, SUB duplicado y UNSUB)")
