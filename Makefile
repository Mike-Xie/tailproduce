LEVELDB_WITH_VERSION=leveldb-1.15.0

.PHONY: test lib clean cleanall love indent

lib: leveldb/libleveldb.a
	make -f Makefile.tailproduce

clean:
	(make -f Makefile.tailproduce clean; cd test/cpp && make clean)

cleanall: clean
	rm -rf leveldb ${LEVELDB_WITH_VERSION} cereal

test: leveldb/libleveldb.a cereal
	(make -f Makefile.tailproduce; cd test/cpp && make test)

love: lib test

indent:
	find src/ test/ -regextype posix-egrep -regex ".*\.(cc|h)" | xargs clang-format-3.5 -i

# Keep dependencies static.

# From http://code.google.com/p/leveldb/downloads/list
leveldb/libleveldb.a: ${LEVELDB_WITH_VERSION}.tar.gz
	tar xzf ${LEVELDB_WITH_VERSION}.tar.gz
	ln -sf ${LEVELDB_WITH_VERSION} leveldb
	(cd leveldb ; CXX=clang++ make)

# From git clone git@github.com:USCiLab/cereal.git
cereal:
	tar xjf cereal.tar.bz2
