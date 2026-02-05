#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
export_dir="chowdsp-slides-test"

ssh ccrma-gate.stanford.edu "rm -R Library/Web/slides/${export_dir}"
scp -r ${SCRIPT_DIR}/web ccrma-gate.stanford.edu:~/Library/Web/slides/${export_dir}

# ssh ccrma-gate.stanford.edu "cd Library/Web/slides && bash .gen-tree.sh"
