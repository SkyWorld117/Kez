#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-03
#SBATCH --output=fromager-test-03.out
#SBATCH --error=fromager-test-03.err
#SBATCH --nodelist=piora1
#SBATCH --exclusive

# Test 03 checks the autotools toolchain

set -Eeuo pipefail

fgr cellar create autotools-test
fgr install libx11 --cellar autotools-test

set +Eeuo pipefail