# Generate executable
sshell: sshell.o
	gcc -g -Wall -Wextra -Werror -o sshell sshell.o 

# Generate object file from C file
sshell.o: sshell.c 
	gcc -g -Wall -Wextra -Werror -c -o sshell.o sshell.c

# Remove object files and executable
clean:
	rm -f sshell *.o 