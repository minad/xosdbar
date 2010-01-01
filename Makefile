osdbar: osdbar.c
	gcc -Wall -o osdbar osdbar.c -lxosd

clean:
	rm -f osdbar
