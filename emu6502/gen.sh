#!/bin/sh

rm -f *.inc *.gen.tmp || exit 1

# instrdef.inc

grep '^[0-9A-Fa-f]' tbl_inst_set.data \
  | sed 's/ *$//'  \
  > instset.gen.tmp

luajit gen_instr_set.lua > instrdef.inc
