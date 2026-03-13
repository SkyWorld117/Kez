#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-02
#SBATCH --output=fromager-test-02.out
#SBATCH --error=fromager-test-02.err
#SBATCH --nodelist=piora1
#SBATCH --exclusive

# Test 02 checks the CMake toolchain

set -Eeuo pipefail

fgr cellar create cmake-test
fgr install root --cellar cmake-test

set +Eeuo pipefail