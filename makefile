simple_sc_exe : Fiber.o simple_fiber_scheduler.cpp 
	g++ -o simple_sc_exe Fiber.o simple_fiber_scheduler.cpp 
Fiber.o : Fiber.h Fiber.cpp Comm.h
	g++ -c Fiber.o Fiber.cpp
.CEN : clean
clean :
	rm Fiber.o simple_sc_exe Comm.h.gch Fiber.h.gch