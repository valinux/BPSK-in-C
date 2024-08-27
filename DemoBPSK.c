#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define PI 3.14159265358979323846

// Function to read WAV file
void read_wav_file(const char *file_path, int *fs, int16_t **data, int *length) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        exit(1);
    }

    fseek(file, 24, SEEK_SET);  // Skip to sample rate
    fread(fs, sizeof(int), 1, file);

    fseek(file, 40, SEEK_SET);  // Skip to data length
    fread(length, sizeof(int), 1, file);

    *length = *length / sizeof(int16_t);  // Convert byte length to sample count

    if (*length <= 0) {
        fprintf(stderr, "Invalid WAV file length\n");
        fclose(file);
        exit(1);
    }

    *data = (int16_t *)malloc(*length * sizeof(int16_t));
    if (!(*data)) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        exit(1);
    }

    fread(*data, sizeof(int16_t), *length, file);
    fclose(file);
}

// Function to demodulate BPSK
void demodulate_bpsk(int16_t *signal, int fs, int f0, int baud, int length, int *bits, int *bit_length) {
    int Ns = fs / baud;
    *bit_length = length / Ns;

    printf("Ns: %d, bit_length: %d\n", Ns, *bit_length);

    double *carrier = (double *)malloc(length * sizeof(double));
    if (carrier == NULL) {
        fprintf(stderr, "Failed to allocate memory for carrier\n");
        exit(1);
    }
    
    double *demodulated = (double *)malloc(length * sizeof(double));
    if (demodulated == NULL) {
        fprintf(stderr, "Failed to allocate memory for demodulated\n");
        free(carrier);
        exit(1);
    }

    printf("Entering loop to generate carrier and demodulate signal...\n");
    
    for (int i = 0; i < length; i++) {
        if (i % 100000 == 0) {
            printf("Processing index i=%d/%d\n", i, length);
        }
        double t = (double)i / fs;
        carrier[i] = cos(2 * PI * f0 * t);
        demodulated[i] = (double)signal[i] * carrier[i];
    }

    printf("Carrier generation and demodulation complete.\n");

    // Apply moving average to smooth the signal
    for (int i = 0; i < *bit_length; i++) {
        double sum = 0.0;
        int count = 0;
        for (int j = 0; j < Ns && (i * Ns + j) < length; j++) {
            sum += demodulated[i * Ns + j];
            count++;
        }
        bits[i] = (count > 0 && sum > 0) ? 1 : 0;

        // Debugging: Check progress and bit values
        if (i % 10000 == 0) {
            printf("Processed bit i=%d/%d, bit=%d\n", i, *bit_length, bits[i]);
        }
    }

    printf("Demodulation and moving average complete.\n");

    free(carrier);
    free(demodulated);
}

// Function to convert binary data to text
void binary_to_text(int *bits, int bit_length, const char *output_txt) {
    int byte_length = (bit_length + 7) / 8;  // Round up to nearest byte
    unsigned char *bytes = (unsigned char *)calloc(byte_length, sizeof(unsigned char));

    if (bytes == NULL) {
        fprintf(stderr, "Failed to allocate memory for bytes\n");
        exit(1);
    }

    for (int i = 0; i < bit_length; i++) {
        if (bits[i]) {
            bytes[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    FILE *file = fopen(output_txt, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open output file: %s\n", output_txt);
        free(bytes);
        exit(1);
    }

    fwrite(bytes, sizeof(unsigned char), byte_length, file);
    fclose(file);

    free(bytes);
}

// New function to parse command-line arguments
void parse_arguments(int argc, char* argv[], int* fs, int* baud, int* f0, char** input_file, char** output_file) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-fs") == 0 && i + 1 < argc) {
            *fs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-baud") == 0 && i + 1 < argc) {
            *baud = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-f0") == 0 && i + 1 < argc) {
            *f0 = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            *input_file = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            *output_file = argv[++i];
        }
    }

    if (*input_file == NULL || *output_file == NULL) {
        fprintf(stderr, "Usage: %s -i <input_wav> -o <output_txt> [-fs <sampling_rate>] [-baud <symbol_rate>] [-f0 <carrier_frequency>]\n", argv[0]);
        exit(1);
    }
}

// Main function
int main(int argc, char* argv[]) {
    int fs = 44100;  // Default values
    int baud = 1000;
    int f0 = 2000;
    char *input_wav = NULL;
    char *output_txt = NULL;

    parse_arguments(argc, argv, &fs, &baud, &f0, &input_wav, &output_txt);

    int length;
    int16_t *signal;

    printf("Reading WAV file...\n");
    read_wav_file(input_wav, &fs, &signal, &length);
    printf("WAV file read successfully. fs=%d, length=%d\n", fs, length);

    int *demodulated_bits = (int *)malloc(length * sizeof(int));
    if (demodulated_bits == NULL) {
        fprintf(stderr, "Failed to allocate memory for demodulated_bits\n");
        free(signal);
        exit(1);
    }

    int bit_length;
    demodulate_bpsk(signal, fs, f0, baud, length, demodulated_bits, &bit_length);

    binary_to_text(demodulated_bits, bit_length, output_txt);

    free(signal);
    free(demodulated_bits);

    printf("Demodulation complete. Recovered text saved to %s\n", output_txt);

    return 0;
}
