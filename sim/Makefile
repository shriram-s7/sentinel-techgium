all: host longsim

host: host.c ../firmware/sentinel.c ../firmware/sentinel.h
	gcc -O2 -I../firmware host.c ../firmware/sentinel.c -o host

longsim: longsim.c ../firmware/sentinel.c ../firmware/sentinel.h
	gcc -O2 -I../firmware longsim.c ../firmware/sentinel.c -o longsim -lm

run: all
	python3 sim.py 2027 && python3 ablation.py && python3 longrun.py

clean:
	rm -f host longsim host.exe longsim.exe
