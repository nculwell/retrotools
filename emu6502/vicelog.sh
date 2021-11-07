#!/bin/sh

cat ../monitor.log \
  | perl -pe '$_=uc;s/ +\d+ *$//' \
  | grep -v '^...[EF]'

#  | grep -v 'I..$'

