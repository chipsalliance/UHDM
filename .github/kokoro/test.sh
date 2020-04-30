#!/bin/bash

set -e

cd github/$KOKORO_DIR/

source ./.github/kokoro/steps/hostsetup.sh
source ./.github/kokoro/steps/hostinfo.sh
source ./.github/kokoro/steps/git.sh

echo
echo "==========================================="
echo "Building UHDM"
echo "-------------------------------------------"
(
	make -j8
	sudo make install
)
echo "-------------------------------------------"

echo
echo "==========================================="
echo "Executing UHDM tests"
echo "-------------------------------------------"
(
	make test-junit
)
echo "-------------------------------------------"
echo
echo "========================================"
echo "Tests logs"
echo "----------------------------------------"
(
	cat build/*test_results.xml
)
echo "----------------------------------------"
