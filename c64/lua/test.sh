#!/bin/sh

ALL_PASSED=1
for TEST in "$(dirname "$0")/"test_*.lua ; do
  echo $(basename "$TEST")
  luajit "$TEST" || ALL_PASSED=0
done
if [ 1 = $ALL_PASSED ] ; then
  echo "ALL TESTS PASSED"
else
  echo "SOME TESTS FAILED"
fi
