#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-04
#SBATCH --output=fromager-test-04.out
#SBATCH --error=fromager-test-04.err
#SBATCH --nodelist=piora1
#SBATCH --exclusive

# Test 04 checks the makefile toolchain

set -Eeuo pipefail

fgr cellar create makefile-test
fgr install goblinhmcsim --cellar makefile-test

set +Eeuo pipefail