CC        = gcc
SRC       = main.c
TARGET    = toss
DST_ADDR  = 127.0.0.1
BIND_ADDR = 0.0.0.0
HOST      = 12345
INPUT     = ./Makefile
TMP       = /tmp/toss-received

FNAME_LEN = 4 # uint32_t
FSIZE     = 8 # uint64_t

default:
	@printf "=> building...\n"
	@$(CC) -o $(TARGET) $(SRC)
	@printf "=> done.\n"

run: default
	@printf "=> running test...\n"
	@nc -l $(BIND_ADDR) $(HOST) > $(TMP) &
	@sleep 0.25
	./$(TARGET) --send $(DST_ADDR) --port $(HOST) $(INPUT)
	@printf "%s received\n" "$$(wc -c < $(TMP)) bytes"

	@input=$(INPUT);                                                                                                           \
     name=$${input##*/};                                                                                                       \
     if [ $$(wc -c < $(TMP)) -eq $$(( $$(wc -c < $(INPUT)) + $(FNAME_LEN) + $$(echo -n "$$name" | wc -c) + $(FSIZE) )) ]; then \
         echo "=> success.";                                                                                                   \
     else                                                                                                                      \
         echo "=> failed.";                                                                                                    \
     fi

ins:
	@printf "=> running inspect on $(TMP)...\n"
	@xxd -g 1 -R always $(TMP) | bat
	@printf "=> done.\n"

clean:
	rm -f $(TARGET) $(TMP)
