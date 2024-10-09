exe: test.cpp Fiber.o threadPoll_fiber_scheduler.o  Semaphore.o Thread.o
	g++ -o exe -g test.cpp   threadPoll_fiber_scheduler.o Fiber.o Semaphore.o Thread.o 

threadPoll_fiber_scheduler.o: threadPoll_fiber_scheduler.h threadPoll_fiber_scheduler.cpp Comm.h Fiber.h
	g++ -g -c threadPoll_fiber_scheduler.cpp

Fiber.o: Fiber.h Fiber.cpp Comm.h threadPoll_fiber_scheduler.h
	g++ -g -c Fiber.cpp

Semaphore.o: Semaphore.cpp Semaphore.h
	g++ -g -c Semaphore.cpp

Thread.o: Thread.h Thread.cpp
	g++ -g -c Thread.cpp -lpthread

clean: 
	rm *.o exe *.gch