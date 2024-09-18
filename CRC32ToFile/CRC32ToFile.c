#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// To compute CRC32 for a file
// https://simplycalc.com/crc32-file.php#

unsigned int crc32b(unsigned char *message) {
    int i, j;
    unsigned int byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    while (message[i] != 0) {
        byte = message[i];            // Get next byte
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--) {    // Do eight times
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
        i = i + 1;
    }
    return ~crc;
}

// Function to compute the CRC of a file
unsigned int compute_file_crc(FILE *file) {
    unsigned char buffer[1024];
    size_t bytesRead;
    unsigned int crc = 0xFFFFFFFF;
    
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytesRead; i++) {
            crc ^= buffer[i];
            for (int j = 7; j >= 0; j--) {
                unsigned int mask = -(crc & 1);
                crc = (crc >> 1) ^ (0xEDB88320 & mask);
            }
        }
    }

    return ~crc;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r+b"); // Open file for reading and writing
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Compute CRC of the file
    unsigned int crc = compute_file_crc(file);
    
    
    // Create a string to hold the hexadecimal representation of the CRC
    char crc_hex[9]; // 8 characters for CRC + null terminator
    snprintf(crc_hex, sizeof(crc_hex), "%08X", crc);

    fseek(file, 0, SEEK_END); // Move to the end of the file

    // Append the CRC value as an 8-character hexadecimal string
    if (fwrite(crc_hex, sizeof(char), strlen(crc_hex), file) != strlen(crc_hex)) {
        perror("Error writing CRC to file");
        fclose(file);
        return 1;
    }

    fclose(file);
    printf("CRC appended successfully: 0x%08X\n", crc);
    return 0;
}
