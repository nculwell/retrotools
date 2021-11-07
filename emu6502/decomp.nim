# ACS Forth Decompiler
# vim: ai

import std/strformat
import tables

const RAM_SIZE = 64 * 1024 # 64K

type
  TargetFlag = uint16
  Decomp = tuple
    ram: array[RAM_SIZE, uint8]
    refType: array[RAM_SIZE, uint8]
    targetFlags: array[RAM_SIZE, TargetFlag]
    label: Table[string, int]

const
  TF_SCANNED = 0b1

type
  R = enum

    none, unknown, cont, cont_var,

    val_lit, val_branch_offset, val_str_lenpfx, val_var,

    colon_ref, const_ref, var_ref, use_ref,
    colon_def, const_def, var_def, use_def,

    include forth_words.ids

var
  token_names: seq[string]

proc fail(msg: string) =
  stderr.writeLine(msg)
  quit(1)

proc warn(msg: string) =
  stderr.write("WARNING: ")
  stderr.writeLine(msg)

proc trace(msg: string) =
  stderr.writeLine(msg)

proc add_label(label: string): string =
  
