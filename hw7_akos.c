#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>


#define BUFFER_SIZE 32

int main(const int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    struct stat input_stat;
    if (stat(input_filename, &input_stat) == -1) {
        perror("Error getting input file stats");
        return 1;
    }

    const int input_fd = open(input_filename, O_RDONLY);
    if (input_fd == -1) {
        perror("Error opening input file");
        return 1;
    }

    const int output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (output_fd == -1) {
        perror("Error opening output file");
        close(input_fd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;

    while ((bytes_read = read(input_fd, buffer, BUFFER_SIZE)) > 0) {
      const char *ptr = buffer;
        ssize_t remaining = bytes_read;

        while (remaining > 0) {
            bytes_written = write(output_fd, ptr, remaining);
            if (bytes_written <= 0) {
                perror("Error writing to output file");
                close(input_fd);
                close(output_fd);
                return 1;
            }
            remaining -= bytes_written;
            ptr += bytes_written;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading input file");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    if (fchmod(output_fd, input_stat.st_mode) == -1) {
        perror("Error setting output file permissions");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    close(input_fd);
    close(output_fd);

    return 0;
}