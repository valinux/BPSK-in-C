#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846

// Configuration
#define FS 44100  // sampling rate
#define BAUD 1000 // symbol rate
#define F0 2000   // carrier frequency

// Updated function prototypes
void text_to_binary(const char* input_file, bool** bits, int* num_bits);
void generate_bpsk_signal(bool* input_bits, int num_bits, short** signal, int* signal_length, int fs, int baud, int f0);
void write_wav_file(const char* output_file, short* signal, int signal_length, int fs);
void parse_arguments(int argc, char* argv[], int* fs, int* baud, int* f0, char** input_file, char** output_file);

int main(int argc, char* argv[]) {
    int fs = FS;
    int baud = BAUD;
    int f0 = F0;
    char* input_file;
    char* output_file;

    parse_arguments(argc, argv, &fs, &baud, &f0, &input_file, &output_file);

    bool* input_bits;
    int num_bits;
    text_to_binary(input_file, &input_bits, &num_bits);

    short* bpsk_signal;
    int signal_length;
    generate_bpsk_signal(input_bits, num_bits, &bpsk_signal, &signal_length, fs, baud, f0);

    write_wav_file(output_file, bpsk_signal, signal_length, fs);

    printf("BPSK signal generated and saved to %s\n", output_file);

    free(input_bits);
    free(bpsk_signal);

    return 0;
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
        fprintf(stderr, "Usage: %s -i <input_file> -o <output_file> [-fs <sampling_rate>] [-baud <symbol_rate>] [-f0 <carrier_frequency>]\n", argv[0]);
        exit(1);
    }
}

void text_to_binary(const char* input_file, bool** bits, int* num_bits) {
    FILE* file = fopen(input_file, "rb");
    if (!file) {
        fprintf(stderr, "Error opening input file\n");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(file_size);
    fread(buffer, 1, file_size, file);
    fclose(file);

    *num_bits = file_size * 8;
    *bits = malloc(*num_bits * sizeof(bool));

    for (int i = 0; i < file_size; i++) {
        for (int j = 0; j < 8; j++) {
            (*bits)[i * 8 + j] = (buffer[i] >> (7 - j)) & 1;
        }
    }

    free(buffer);
}

void generate_bpsk_signal(bool* input_bits, int num_bits, short** signal, int* signal_length, int fs, int baud, int f0) {
    int Ns = fs / baud;
    *signal_length = num_bits * Ns;
    *signal = malloc(*signal_length * sizeof(short));

    for (int i = 0; i < num_bits; i++) {
        double symbol = input_bits[i] ? 1.0 : -1.0;
        for (int j = 0; j < Ns; j++) {
            int idx = i * Ns + j;
            double t = (double)idx / fs;
            double carrier = cos(2 * PI * f0 * t);
            (*signal)[idx] = (short)(symbol * carrier * 32767);
        }
    }
}

void write_wav_file(const char* output_file, short* signal, int signal_length, int fs) {
    FILE* file = fopen(output_file, "wb");
    if (!file) {
        fprintf(stderr, "Error opening output file\n");
        exit(1);
    }

    // WAV file header
    const int header_size = 44;
    const int data_size = signal_length * sizeof(short);
    const int file_size = header_size + data_size;

    unsigned char header[44];  // Fixed size array

    // RIFF chunk descriptor
    memcpy(header, "RIFF", 4);
    header[4] = file_size & 0xff;
    header[5] = (file_size >> 8) & 0xff;
    header[6] = (file_size >> 16) & 0xff;
    header[7] = (file_size >> 24) & 0xff;
    memcpy(header + 8, "WAVE", 4);

    // fmt sub-chunk
    memcpy(header + 12, "fmt ", 4);
    header[16] = 16;  // Sub-chunk size (16 for PCM)
    header[17] = 0;
    header[18] = 0;
    header[19] = 0;
    header[20] = 1;   // Audio format (1 for PCM)
    header[21] = 0;
    header[22] = 1;   // Number of channels (1 for mono)
    header[23] = 0;
    
    // Sample rate
    header[24] = fs & 0xff;
    header[25] = (fs >> 8) & 0xff;
    header[26] = (fs >> 16) & 0xff;
    header[27] = (fs >> 24) & 0xff;
    
    // Byte rate
    int byte_rate = fs * 2;  // Sample rate * num channels * bytes per sample
    header[28] = byte_rate & 0xff;
    header[29] = (byte_rate >> 8) & 0xff;
    header[30] = (byte_rate >> 16) & 0xff;
    header[31] = (byte_rate >> 24) & 0xff;
    
    header[32] = 2;   // Block align
    header[33] = 0;
    header[34] = 16;  // Bits per sample
    header[35] = 0;

    // data sub-chunk
    memcpy(header + 36, "data", 4);
    header[40] = data_size & 0xff;
    header[41] = (data_size >> 8) & 0xff;
    header[42] = (data_size >> 16) & 0xff;
    header[43] = (data_size >> 24) & 0xff;

    fwrite(header, 1, header_size, file);
    fwrite(signal, sizeof(short), signal_length, file);
    fclose(file);
}