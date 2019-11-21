all:
	$(MAKE) -C runtime
	$(MAKE) -C handcomp_test
	$(MAKE) -C bench
	$(MAKE) -C application_requests

rebuild:
	$(MAKE) -C runtime clean
	$(MAKE) -C runtime
	$(MAKE) -C handcomp_test clean
	$(MAKE) -C handcomp_test
	$(MAKE) -C bench clean
	$(MAKE) -C bench
	$(MAKE) -C application_requests clean
	$(MAKE) -C application_requests

clean:
	$(MAKE) -C handcomp_test clean
	$(MAKE) -C bench clean
	$(MAKE) -C runtime clean
	$(MAKE) -C application_requests clean
