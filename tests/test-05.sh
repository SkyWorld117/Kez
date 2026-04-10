#!/usr/bin/env bash
#SBATCH --job-name=fromager-test-05
#SBATCH --output=fromager-test-05.out
#SBATCH --error=fromager-test-05.err
#SBATCH --nodelist=piora1
#SBATCH --exclusive

# Test 05 checks the ability to install multiple packages at once

set -Eeuo pipefail

fgr cellar create multipkg-test
fgr template python libx11
fgr install python libx11 --cellar multipkg-test

set +Eeuo pipefail