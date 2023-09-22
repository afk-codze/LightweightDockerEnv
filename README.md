# LightweightDockerEnv
A lightweight container runtime, It offers the core essentials of Docker-like functionalities with an emphasis on process isolation through PID namespaces.

## **How to run the program**
The program can be run using the following syntax:

./name_of_program run <image> <command> <arg1> <arg2> ...

Replace <image>, <command>, <arg1>, and <arg2> with the appropriate arguments based on your requirements.

## **Valgrind report**
The following is a recent memory analysis report for the program: 

==56512== Memcheck, a memory error detector

==56512== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.

==56512== Using Valgrind-3.18.1 and LibVEX; rerun with -h for copyright info

==56512== Command: ./name_of_program run library/nginx bin/ls

[... execution output ...]

==56512== 

==56512== HEAP SUMMARY:

==56512==     in use at exit: 0 bytes in 0 blocks

==56512==   total heap usage: 1,312 allocs, 1,312 frees, 105,763 bytes allocated

==56512== 

==56512== All heap blocks were freed -- no leaks are possible

==56512== 

==56512== For lists of detected and suppressed errors, rerun with: -s

==56512== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

## **Potential Technical Improvements:**
**1. Layer Download Optimization:** The current approach waits for traffic redirection before initiating the download of Docker Layers, as URLs constructed from digests in image manifests often necessitate redirection. A more efficient mechanism could directly assess if a layer's URL is immediately accessible and, if so, bypass redirection altogether for faster retrieval.

**2. Enhanced Filesystem Mounting:** Introducing mounting of essential filesystems like /proc inside the new root can prove beneficial. Many system utilities, such as ps and top, rely heavily on /proc. Moreover, to accommodate Docker images that require access to specialized device interfaces, mounting /dev could be implemented.

**3. Incorporating the Configuration Blob:** Including the configuration blob directly within the filesystem could be explored. This blob, found in image manifests, is a JSON structure containing valuable metadata about the Docker image. It encompasses details like default commands, environment variable settings, and other Docker-centric specifications.

**4. Transitioning Root for Security:** While chroot is currently employed, considering a shift to pivot_root may bolster security. This transition would address potential vulnerabilities that can arise with chroot, offering a more secure container runtime environment.
