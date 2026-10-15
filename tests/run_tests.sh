#!/usr/bin/env bash
# Ejecuta todos los casos de tests/*/*.cringe.
# Cada archivo declara lo que se espera en comentarios al inicio:
#   // codigo: N        codigo de salida esperado (0 correcto, 1 lexico, 2 sintactico, 3 semantico)
#   // mensaje: texto   fragmento que debe aparecer en la salida (se pueden poner varias lineas)

cd "$(dirname "$0")/.." || exit 1
BIN=./cringe
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

for f in tests/*/*.cringe; do
    esperado=$(sed -n 's|^// codigo: *\([0-9]*\).*|\1|p' "$f" | head -1)
    mensajes=$(sed -n 's|^// mensaje: *||p' "$f")

    salida=$("$BIN" "$f" -o "$TMP" 2>&1)
    codigo=$?

    ok=1
    faltante=""
    [ "$codigo" = "$esperado" ] || ok=0
    while IFS= read -r m; do
        [ -z "$m" ] && continue
        if ! printf '%s' "$salida" | grep -qF -- "$m"; then
            ok=0
            faltante="$m"
            break
        fi
    done <<< "$mensajes"

    if [ $ok = 1 ]; then
        pass=$((pass + 1))
        echo "OK    $f"
    else
        fail=$((fail + 1))
        echo "FALLO $f (esperado codigo $esperado, obtenido $codigo)"
        [ -n "$faltante" ] && echo "      mensaje esperado que no aparece: $faltante"
        printf '%s\n' "$salida" | sed 's/^/      /' | head -8
    fi
done

echo
echo "Resultado: $pass correctas, $fail con fallo"
[ $fail = 0 ]
