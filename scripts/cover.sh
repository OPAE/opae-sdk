#!/bin/bash -e

set -o xtrace

function finish() {
	echo "making coverage report"
	find testing -iname "*.gcda" -exec chmod 664 '{}' \;
	find testing -iname "*.gcno" -exec chmod 664 '{}' \;

	lcov --directory testing --capture --output-file coverage.info 2> /dev/null

	lcov -a coverage.base -a coverage.info --output-file coverage.total
	lcov --remove coverage.total '/usr/**' '*/**/CMakeFiles*' '/usr/*' 'safe_string/**' 'pybind11/*' 'testing/**' --output-file coverage.info.cleaned
	genhtml --function-coverage -o coverage_report coverage.info.cleaned 2> /dev/null

}
trap "finish" EXIT

mkdir -p unittests
cd unittests

if [ ! -f CMakeCache.txt ]; then
	cmake .. -DOPAE_PYTHON_VERSION=2.7 -DCMAKE_BUILD_TYPE=Coverage -DBUILD_TESTS=ON -DBUILD_LIBOPAE_PY=ON
fi

mkdir -p coverage_files
rm -rf coverage_files/*

echo "Making tests"
make -j4 test_unit xfpga modbmc fpgad-xfpga

lcov --directory . --zerocounters
lcov -c -i -d . -o coverage.base 2> /dev/null

LD_LIBRARY_PATH=${PWD}/lib \
CTEST_OUTPUT_ON_FAILURE=1 \
OPAE_EXPLICIT_INITIALIZE=1 \
ctest -j3 --timeout 60

