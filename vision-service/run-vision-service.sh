#!/bin/bash
# Script wrapper pour lancer le vision-service avec les bonnes bibliothèques

export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/build/vision-service" "$@"
