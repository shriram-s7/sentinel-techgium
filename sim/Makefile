host: host.c ../firmware/sentinel.c ../firmware/sentinel.h
	gcc -O2 -I../firmware host.c ../firmware/sentinel.c -o host

run: host
	python3 sim.py

clean:
	rm -f host host.exe
