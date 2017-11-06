all: multiblock database

multiblock: multiblock.c
		gcc -o multiblock multiblock.c -lnetfilter_queue -l sqlite3

database: db.c
		gcc -o database db.c -l sqlite3

clean:
		rm multiblock database
