#define _GNU_SOURCE // --> to enable the unshare() function, which is a Linux-specific function not typically available in standard C
#include <sched.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/mount.h> 
#include <linux/unistd.h>
#include <sys/syscall.h>
#include "networking.h"

//UTILITIES
void print_current_directory(){
	char cwd[1024]; // Create a buffer to store the current directory
   	if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current directory: %s\n", cwd);
    } else {
       	perror("getcwd");
		return;
   	}
}

int delete_current_directory() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return -1;
    }
    // Change to the parent directory
    if (chdir("..") == -1) {
        perror("chdir");
        return -1;
    }
    // Delete the original directory
    if (rmdir(cwd) == -1) {
        perror("rmdir");
        return -1;
    }
    return 0;
}

int copy_image_file(char *source_path, char *destination_path){
	FILE *source_file, *destination_file;
	if((source_file = fopen(source_path, "r")) == NULL){
		perror("Could not open source file");
		return -1;
	}
	if((destination_file = fopen(destination_path, "w+")) == NULL){
		perror("Could not open destination file");
		return -1;
	}
	//lets write the destination
	char c = fgetc(source_file);
	while(c != EOF){
		fputc(c, destination_file);
		c = fgetc(source_file);
	}
	fclose(destination_file);
	fclose(source_file);
	return 0;
}

//It is preferable for several reasons. The main one is security --> With chroot, processes can potentially escape the chroot jail, especially if they have root privileges. This is because chroot changes only the apparent root directory and does not provide a full filesystem isolation.
																	//On the other hand, pivot_root is designed to work with namespaces (specifically the mount namespace in Linux) to provide better filesystem isolation.
int setup_environment(char *docker_image){
	char template[] = "/tmp/mydir_XXXXXX";

    char* dir_name = mkdtemp(template);
    if (!dir_name) {
        perror("Error creating temporary directory");
        return 1;
    }

	int res = get_image(docker_image, dir_name);

	// chroot to activate our new environment --> its better to use pivot_root (more secure)
  	if (chdir(dir_name) || chroot(dir_name)) {
    	perror("Error, could not chroot to new directory");
    	return -1;
  	}
	return 0;
}

// Usage: ./name_of_program run <image> <command> <arg1> <arg2> ...
int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);

    int pipe_stdout[2];
    int pipe_stderr[2];
    
    if(pipe(pipe_stdout) == -1 || pipe(pipe_stderr) == -1){
        perror("error creating pipes!");
        return -1;
    }
    
    char *command = argv[3];
    char *docker_image = argv[2];

    unshare(CLONE_NEWPID); 

    int pid = fork();

    if (pid == -1) {
        perror("Error forking!");
        return -1;
    }
    
    if (pid == 0) {
        close(pipe_stdout[0]);
        close(pipe_stderr[0]);

        dup2(pipe_stdout[1], STDOUT_FILENO);
        dup2(pipe_stderr[1], STDERR_FILENO);

        setup_environment(docker_image);

        int res_exec = execv(command, &argv[3]);
        if(res_exec == -1){
            perror("\nexec error");
            _exit(-1);
        }
        close(pipe_stdout[1]);
        close(pipe_stderr[1]);
    } else {
        close(pipe_stdout[1]);
        close(pipe_stderr[1]);

        fd_set read_fds;
        char buf[1024];
        int bytes;

        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(pipe_stdout[0], &read_fds);
            FD_SET(pipe_stderr[0], &read_fds);

            int max_fd = (pipe_stdout[0] > pipe_stderr[0]) ? pipe_stdout[0] : pipe_stderr[0];

            int ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
            
            if (ret == -1) {
                perror("select");
                break;
            } else if (ret) {
                if (FD_ISSET(pipe_stdout[0], &read_fds)) {
                    bytes = read(pipe_stdout[0], buf, sizeof(buf));
                    if (bytes <= 0) break;  // EOF or error
                    write(STDOUT_FILENO, buf, bytes);
                }
                if (FD_ISSET(pipe_stderr[0], &read_fds)) {
                    bytes = read(pipe_stderr[0], buf, sizeof(buf));
                    if (bytes <= 0) break;  // EOF or error
                    write(STDERR_FILENO, buf, bytes);
                }
            }
        }

        int status;
        wait(&status);
        close(pipe_stdout[0]);
        close(pipe_stderr[0]);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
    }
    return 0;
}


