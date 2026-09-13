CC  = gcc
SRC = main.c
OUT = toss

default:
	$(CC) -o $(OUT) $(SRC)

run: default
	nc -l 127.0.0.1 12345 &
	./$(OUT) 127.0.0.1 main.c

clean: 
	rm -rf $(OUT)
