include ../config.mk

.PHONY: all check memcheck clean

all:
	gcc cilk_request.c -o cilk_request
	gcc cilk_request_periodic.c -pthread -o cilk_request_periodic
	gcc cilk_request_sporadic.c -o cilk_request_sporadic
	gcc bing_h_request.c -lm -o bing_h_request
	gcc basic_test_request.c -lm -o basic_test_request
	gcc james_test.c -lm -pthread -o james_test

clean:
	rm -f *.o *~ cilk_request
	rm -f *.o *~ cilk_request_periodic
	rm -f *.o *~ cilk_request_sporadic
	rm -f *.o *~ bing_h_request
	rm -f *.o *~ basic_test_request
	rm -f *.o *~ james_test
