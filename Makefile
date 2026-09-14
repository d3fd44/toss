CC  = gcc
SRC = main.c
OUT = toss
DST = 127.0.0.1
BND = 0.0.0.0
PRT = 12345
TMP = /tmp/toss-received

default:
	@printf "building...\n"
	$(CC) -o $(OUT) $(SRC)

run: default
	@printf "running test\n"
	nc -l $(BND) $(PRT) > $(TMP) &
	./$(OUT) -s $(DST) -p $(PRT) $(SRC)
	@printf "%s received\n" "$$(wc -c < $(TMP))"
	@printf "%s actual size\n" "$$(wc -c < $(SRC))"

ins:
	xxd -g 1 -R always $(TMP) | bat

clean:
	rm -f $(OUT) $(TMP)
