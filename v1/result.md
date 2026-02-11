// lock
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_malloc_free
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_malloc_free_change_thread
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_measurement
No overlapping allocated regions found!
Test passed
Execution Time = 0.047158 seconds
Data Segment Size = 41942160 bytes



//no-lock 1st test
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_malloc_free
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_malloc_free_change_thread
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_measurement
No overlapping allocated regions found!
Test passed
Execution Time = 0.088846 seconds
Data Segment Size = 42146944 bytes
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# 

//no-lock test2
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_malloc_free
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_malloc_free_change_thread
No overlapping allocated regions found!
Test passed
root@VM-16-2-ubuntu:~/Thread-Safe-Malloc/v1/thread_tests# ./thread_test_measurement
No overlapping allocated regions found!
Test passed
Execution Time = 0.115934 seconds
Data Segment Size = 41606864 bytes