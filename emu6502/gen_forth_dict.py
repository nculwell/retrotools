#!/usr/bin/python3

SOURCE_FILENAME = "forth_words.txt"
DEST_FILENAME_DEFS  = "forth_words_defs.inc"
DEST_FILENAME_ADDRS = "forth_words_addrs.inc"
DEST_FILENAME_NAMES = "forth_words_names.inc"

with open(SOURCE_FILENAME) as f:
    lines = [ ln for ln in ( l.strip() for l in f.readlines() ) if len(ln)>0 and not ln.startswith("#") ]

with open(DEST_FILENAME_DEFS, "w") as fd:
    with open(DEST_FILENAME_ADDRS, "w") as fa:
        with open(DEST_FILENAME_NAMES, "w") as fn:
            for ln in lines:
                fields = ln.split("\t")
                name = fields[0]
                addrs = [ "case 0x%s: " % a.strip() for a in fields[1].split(",") ]
                if len(fields) > 2:
                    dispName = fields[2]
                else:
                    dispName = name
                print("R_"+name+",", file=fd)
                print("".join(addrs) + "return R_" + name + ";", file=fa)
                print('TOKEN_NAMES[R_%s] = "%s";' % (name, dispName), file=fn)

