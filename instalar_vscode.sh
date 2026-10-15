#!/usr/bin/env bash
# Instala la extension de resaltado de Cringe en VS Code (o VSCodium).
set -e

ORIGEN="$(cd "$(dirname "$0")" && pwd)/vscode-cringe"

if [ -d "$HOME/.vscode" ] || command -v code >/dev/null 2>&1; then
    BASE="$HOME/.vscode/extensions"
elif [ -d "$HOME/.vscode-oss" ] || command -v codium >/dev/null 2>&1; then
    BASE="$HOME/.vscode-oss/extensions"
else
    BASE="$HOME/.vscode/extensions"
fi

DEST="$BASE/local.cringe-lang-0.1.0"
mkdir -p "$DEST"
cp -r "$ORIGEN/." "$DEST/"

echo "Extension instalada en: $DEST"
echo "Ahora cierra VS Code por completo y vuelve a abrirlo."
