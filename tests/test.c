#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int status = system("./build/ascii-fps > ./build/test-output.txt");

    if (status != 0) {
        fprintf(stderr, "Expected exit code 0, got %d\n", status);
        return 1;
    }

    FILE *file = fopen("./build/test-output.txt", "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open test output file\n");
        return 1;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        fprintf(stderr, "No output captured\n");
        return 1;
    }

    fclose(file);

    if (strcmp(buffer, "hello world\n") != 0) {
        fprintf(stderr, "Expected 'hello world', got '%s'\n", buffer);
        return 1;
    }

    printf("All tests passed\n");
    return 0;
}
