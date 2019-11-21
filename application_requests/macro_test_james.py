import os
import time
import threading

def run_1():
	os.system('./james_test 1 35 1 0 100 30 0 10 0 50 0 1 0 0.9')

def run_2():
	os.system('./james_test 1 35 2 0 100 20 0 5 0 30 0 1 0 0.3')

def run_3():
	os.system('./james_test 1 35 3 0 100 16 0 1 0 10 0 1 0 0.1')

thread1 = threading.Thread(target=run_1, name="1")
thread2 = threading.Thread(target=run_2, name="2")
thread3 = threading.Thread(target=run_3, name="3")

thread1.start()
thread2.start()
thread3.start()

thread1.join()
thread2.join()
thread3.join()