#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
export_dir="chowdsp-slides-test"

ssh chowdsp-web "rm -R web/chowdspweb/slides/${export_dir}"
scp -r ${SCRIPT_DIR}/web chowdsp-web:~/web/chowdspweb/slides/${export_dir}

# ssh ccrma-gate.stanford.edu "cd Library/Web/slides && bash .gen-tree.sh"
