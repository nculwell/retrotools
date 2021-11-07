DIR="$(cd "$(dirname -- "$0")" && pwd)"
OLDPATH=$(mktemp)
echo ":$PATH:" > "$OLDPATH"
for d in apple2 c64 emu6502 ; do
  p="$DIR/$d"
  if ! fgrep -q ":$p:" "$OLDPATH" ; then
    export PATH="$PATH:$p"
  fi
done
rm -f "$OLDPATH"
