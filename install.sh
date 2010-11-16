#!/bin/sh

LUADIR=$HOME/.config/mbblua

for file in bin/*; do
	name=`basename $file`
	cp -v $file $HOME/bin/$name.bin
done

rm -rf $LUADIR
cp -Rv mbbsh/lua $LUADIR

for file in modules/*; do
	install -m555 $file $HOME/lib/mbb/
done

