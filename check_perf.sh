#!/bin/bash

TESTS_FLDR="${PWD}/tests"
TEST_DATA_FLDR="${PWD}/tests/tests_data"
TEST_REPORT_FLDR="${PWD}/tests/tests_report"

TMP_FLDR="$TEST_DATA_FLDR/tmp"
BEFORE_FLDR="$TMP_FLDR/perf/before"
AFTER_FLDR="$TMP_FLDR/perf/after"

TOOL="./build/refactor_tool"
CLANG="clang++-20 -g -O2"

rm -rf "$TMP_FLDR/*"

mkdir -p "$BEFORE_FLDR"
mkdir -p "$AFTER_FLDR"

cp "$TEST_DATA_FLDR/perf_example.cpp" "$BEFORE_FLDR/perf_example.cpp"
cp "$TEST_DATA_FLDR/perf_example.cpp" "$AFTER_FLDR/perf_example.cpp"

$TOOL "$AFTER_FLDR/perf_example.cpp"

$CLANG "$BEFORE_FLDR/perf_example.cpp" -o "$BEFORE_FLDR/perf_example"
$CLANG "$AFTER_FLDR/perf_example.cpp" -o "$AFTER_FLDR/perf_example"

sudo perf stat $BEFORE_FLDR/perf_example 2> "$BEFORE_FLDR/report.log"
sudo perf stat $AFTER_FLDR/perf_example 2> "$AFTER_FLDR/report.log"

cp "$BEFORE_FLDR/report.log" "$TEST_REPORT_FLDR/perf_report_before.log"
cp "$AFTER_FLDR/report.log" "$TEST_REPORT_FLDR/perf_report_after.log"