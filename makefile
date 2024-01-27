
filterSrc = ./http/test.cpp ./log/test.cpp ./log/testLog.cpp ./timer/time_heap.cpp ./timer/timeslice.cpp ./dist/testJson.cpp
libNeed = -lmysqlclient -lpthread -llog4cplus -g
stdNeed = -std=c++2a

allSrc=$(wildcard ./*.cpp ./CGImysql/*.cpp ./http/*.cpp ./timer/*.cpp ./dist/*.cpp ./lfuCache/*.cpp ./memoryPool/*.cpp ./log/*.cpp)
# $(info $(allSrc))
src=$(filter-out $(filterSrc),$(allSrc))
# $(info $(src))
objs=$(patsubst %.cpp, %.o, $(src))
# $(info $(objs))
C++ = g++-13
target = server
$(target) : $(objs)
	$(C++) -o $(target) $(objs) $(libNeed) $(stdNeed)

%.o : %.cpp
	$(C++) -c $< -o $@ $(libNeed) $(stdNeed)

.PHONY:clean
clean:
	rm $(objs) -f
