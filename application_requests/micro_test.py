import os
import time
import random

COUNT = 200

for i in range(0, COUNT):
	os.system('./cilk_request 0 15')
	t = random.randint(1, 9)
	time.sleep(t)
	print("wait", t, "s")
	os.system('./cilk_request 0 14')
	time.sleep(30)