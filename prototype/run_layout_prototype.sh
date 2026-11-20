#!/usr/bin/env bash
set -euo pipefail

prototype_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec qmlscene "$prototype_dir/qml/Main.qml" "$@"
