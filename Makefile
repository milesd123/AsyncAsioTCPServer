# cc = g++ -O3 -std=c++11 -Isrc/external/asio -fsanitize=address -fno-omit-frame-pointer -g
cc = g++ -O3 -std=c++11 -Isrc/external/asio

ProxyServer: src/build/main.o src/build/session.o src/build/varint.o
	$(cc) $^ -o $@

src/build/main.o: src/code/main.cpp
	$(cc) -c $^ -o $@

src/build/session.o: src/code/Session.cpp
	$(cc) -c $^ -o $@

src/build/varint.o: src/code/varint.cpp
	$(cc) -c $^ -o $@

clean:
	rm -rf src/build/*