#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846

// Default parameters
#define DEFAULT_FS 44100    // Sampling rate (Hz)
#define DEFAULT_BAUD 1000   // Symbol rate (symbols per second)
#define DEFAULT_F0 2000     // Carrier frequency (Hz)

// Mode enumeration for selecting modulate/demodulate
typedef enum {
    MODE_UNDEFINED,
    MODE_MODULATE,
    MODE_DEMODULATE
} Mode;

// Function prototypes
void parse_arguments(int argc, char* argv[], Mode *mode, int *fs, int *baud, int *f0, char **input_file, char **output_file);
void text_to_binary(const char* input_file, bool** bits, int* num_bits);
void generate_bpsk_signal(const bool* input_bits, int num_bits, short** signal, int* signal_length, int fs, int baud, int f0);
void write_wav_file(const char* output_file, const short* signal, int signal_length, int fs);

void read_wav_file(const char* file_path, int *fs, int16_t **data, int *length);
void demodulate_bpsk(const int16_t *signal, int fs, int f0, int baud, int length, int *bits, int *bit_length);
void binary_to_text(const int *bits, int bit_length, const char *output_txt);

//
//  parse_arguments: parses command-line arguments
//
void parse_arguments(int argc, char* argv[], Mode *mode, int *fs, int *baud, int *f0, char **input_file, char **output_file) {
    // Set defaults
    *mode = MODE_UNDEFINED;
    *fs = DEFAULT_FS;
    *baud = DEFAULT_BAUD;
    *f0 = DEFAULT_F0;
    *input_file = NULL;
    *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-mode") == 0 && i + 1 < argc) {
            char *modeStr = argv[++i];
            if (strcmp(modeStr, "modulate") == 0) {
                *mode = MODE_MODULATE;
            } else if (strcmp(modeStr, "demodulate") == 0) {
                *mode = MODE_DEMODULATE;
            } else {
                fprintf(stderr, "Unknown mode: %s\n", modeStr);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "-fs") == 0 && i + 1 < argc) {
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

    if (*mode == MODE_UNDEFINED || *input_file == NULL || *output_file == NULL) {
        fprintf(stderr, "Usage: %s -mode <modulate|demodulate> -i <input_file> -o <output_file> [-fs <sampling_rate>] [-baud <symbol_rate>] [-f0 <carrier_frequency>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

//
//  Modulation functions
//

// Converts a text file to a binary array (each bit is stored as a bool)
void text_to_binary(const char* input_file, bool** bits, int* num_bits) {
    FILE *file = fopen(input_file, "rb");
    if (!file) {
        fprintf(stderr, "Error opening input file: %s\n", input_file);
        exit(EXIT_FAILURE);
    }
    // Determine file size
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        perror("ftell failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    rewind(file);

    char *buffer = malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed for file buffer\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    size_t read_count = fread(buffer, 1, file_size, file);
    if (read_count != (size_t)file_size) {
        fprintf(stderr, "Failed to read entire file\n");
        free(buffer);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);

    *num_bits = file_size * 8;
    *bits = malloc(*num_bits * sizeof(bool));
    if (!*bits) {
        fprintf(stderr, "Memory allocation failed for bits\n");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < file_size; i++) {
        for (int j = 0; j < 8; j++) {
            (*bits)[i * 8 + j] = (buffer[i] >> (7 - j)) & 1;
        }
    }
    free(buffer);
}

// Generates a BPSK modulated signal (mono, 16-bit PCM) and stores it in a dynamically allocated array.
void generate_bpsk_signal(const bool* input_bits, int num_bits, short** signal, int* signal_length, int fs, int baud, int f0) {
    int Ns = fs / baud;  // Number of samples per symbol
    *signal_length = num_bits * Ns;
    *signal = malloc(*signal_length * sizeof(short));
    if (!*signal) {
        fprintf(stderr, "Memory allocation failed for signal\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_bits; i++) {
        // Map bit value to symbol amplitude: +1 for bit 1, -1 for bit 0.
        double symbol = input_bits[i] ? 1.0 : -1.0;
        for (int j = 0; j < Ns; j++) {
            int idx = i * Ns + j;
            double t = (double)idx / fs;
            // Multiply the symbol by the carrier waveform
            double carrier = cos(2 * PI * f0 * t);
            // Scale to maximum 16-bit signed integer amplitude
            (*signal)[idx] = (short)(symbol * carrier * 32767);
        }
    }
}

// Writes a mono 16-bit PCM WAV file with a 44-byte header.
void write_wav_file(const char* output_file, const short* signal, int signal_length, int fs) {
    FILE *file = fopen(output_file, "wb");
    if (!file) {
        fprintf(stderr, "Error opening output file: %s\n", output_file);
        exit(EXIT_FAILURE);
    }

    const int header_size = 44;
    const int data_size = signal_length * sizeof(short);
    const int file_size = header_size + data_size;
    unsigned char header[44];

    // RIFF chunk descriptor
    memcpy(header, "RIFF", 4);
    header[4] = file_size & 0xff;
    header[5] = (file_size >> 8) & 0xff;
    header[6] = (file_size >> 16) & 0xff;
    header[7] = (file_size >> 24) & 0xff;
    memcpy(header + 8, "WAVE", 4);

    // fmt sub-chunk (PCM)
    memcpy(header + 12, "fmt ", 4);
    header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;   // Subchunk1Size for PCM
    header[20] = 1;  header[21] = 0;                                     // Audio format (1 = PCM)
    header[22] = 1;  header[23] = 0;                                     // Number of channels (mono)
    // Sample rate
    header[24] = fs & 0xff;
    header[25] = (fs >> 8) & 0xff;
    header[26] = (fs >> 16) & 0xff;
    header[27] = (fs >> 24) & 0xff;
    int byte_rate = fs * 2;  // SampleRate * NumChannels * BitsPerSample/8
    header[28] = byte_rate & 0xff;
    header[29] = (byte_rate >> 8) & 0xff;
    header[30] = (byte_rate >> 16) & 0xff;
    header[31] = (byte_rate >> 24) & 0xff;
    header[32] = 2;  header[33] = 0;                                     // Block align (NumChannels * BitsPerSample/8)
    header[34] = 16; header[35] = 0;                                     // Bits per sample

    // data sub-chunk
    memcpy(header + 36, "data", 4);
    header[40] = data_size & 0xff;
    header[41] = (data_size >> 8) & 0xff;
    header[42] = (data_size >> 16) & 0xff;
    header[43] = (data_size >> 24) & 0xff;

    fwrite(header, 1, header_size, file);
    size_t written = fwrite(signal, sizeof(short), signal_length, file);
    if (written != (size_t)signal_length) {
        fprintf(stderr, "Failed to write WAV data\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

//
//  Demodulation functions
//

// Reads a mono 16-bit PCM WAV file and returns the sample rate and signal data.
// Note: This simple implementation assumes a 44-byte header.
void read_wav_file(const char* file_path, int *fs, int16_t **data, int *length) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        exit(EXIT_FAILURE);
    }

    // Read the sample rate from the header (bytes 24-27)
    if (fseek(file, 24, SEEK_SET) != 0) {
        perror("fseek failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    if (fread(fs, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error reading sample rate\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Read the data subchunk size from the header (bytes 40-43)
    int data_bytes;
    if (fseek(file, 40, SEEK_SET) != 0) {
        perror("fseek failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    if (fread(&data_bytes, sizeof(int), 1, file) != 1) {
        fprintf(stderr, "Error reading data size\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    *length = data_bytes / sizeof(int16_t);
    if (*length <= 0) {
        fprintf(stderr, "Invalid WAV file length\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    *data = malloc(*length * sizeof(int16_t));
    if (!*data) {
        fprintf(stderr, "Memory allocation failed for WAV data\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Move to the beginning of the data (usually at byte 44)
    if (fseek(file, 44, SEEK_SET) != 0) {
        perror("fseek failed");
        free(*data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    if (fread(*data, sizeof(int16_t), *length, file) != (size_t)*length) {
        fprintf(stderr, "Error reading WAV data\n");
        free(*data);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

// Demodulates a BPSK signal. For each symbol (Ns samples) the function multiplies the
// received signal by a locally generated carrier, integrates over the symbol period, and
// decides a 1 if the result is positive (else 0).
void demodulate_bpsk(const int16_t *signal, int fs, int f0, int baud, int length, int *bits, int *bit_length) {
    int Ns = fs / baud;
    *bit_length = length / Ns;

    // Allocate a temporary array for the demodulated (correlated) values.
    double *demodulated = malloc(length * sizeof(double));
    if (!demodulated) {
        fprintf(stderr, "Memory allocation failed for demodulated array\n");
        exit(EXIT_FAILURE);
    }

    // Multiply the received signal with a locally generated carrier.
    for (int i = 0; i < length; i++) {
        double t = (double)i / fs;
        double carrier = cos(2 * PI * f0 * t);
        demodulated[i] = signal[i] * carrier;
    }

    // For each symbol period, integrate the demodulated signal and decide the bit value.
    for (int i = 0; i < *bit_length; i++) {
        double sum = 0.0;
        for (int j = 0; j < Ns && (i * Ns + j) < length; j++) {
            sum += demodulated[i * Ns + j];
        }
        bits[i] = (sum > 0) ? 1 : 0;
    }

    free(demodulated);
}

// Converts a binary array (bits) back into text and writes it to an output file.
void binary_to_text(const int *bits, int bit_length, const char *output_txt) {
    int byte_length = (bit_length + 7) / 8;  // Round up to the nearest byte
    unsigned char *bytes = calloc(byte_length, sizeof(unsigned char));
    if (!bytes) {
        fprintf(stderr, "Memory allocation failed during binary to text conversion\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < bit_length; i++) {
        if (bits[i]) {
            bytes[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    FILE *file = fopen(output_txt, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open output text file: %s\n", output_txt);
        free(bytes);
        exit(EXIT_FAILURE);
    }
    fwrite(bytes, sizeof(unsigned char), byte_length, file);
    fclose(file);
    free(bytes);
}

//
//  Main program
//
int main(int argc, char *argv[]) {
    Mode mode;
    int fs, baud, f0;
    char *input_file, *output_file;

    // Parse command-line arguments. Example usage:
    //   To modulate:   ./bpsk_comm -mode modulate -i input.txt -o output.wav [-fs <rate>] [-baud <rate>] [-f0 <freq>]
    //   To demodulate: ./bpsk_comm -mode demodulate -i input.wav -o output.txt [-baud <rate>] [-f0 <freq>]
    parse_arguments(argc, argv, &mode, &fs, &baud, &f0, &input_file, &output_file);

    if (mode == MODE_MODULATE) {
        // ----- Modulation (TX) -----
        bool *input_bits = NULL;
        int num_bits = 0;
        text_to_binary(input_file, &input_bits, &num_bits);

        short *bpsk_signal = NULL;
        int signal_length = 0;
        generate_bpsk_signal(input_bits, num_bits, &bpsk_signal, &signal_length, fs, baud, f0);

        write_wav_file(output_file, bpsk_signal, signal_length, fs);

        free(input_bits);
        free(bpsk_signal);

        printf("Modulation complete. WAV file saved to %s\n", output_file);

    } else if (mode == MODE_DEMODULATE) {
        // ----- Demodulation (RX) -----
        int16_t *signal = NULL;
        int length = 0;
        read_wav_file(input_file, &fs, &signal, &length);
        // Note: In demodulation the sample rate is taken from the WAV file header.

        int Ns = fs / baud;
        int bit_length = length / Ns;
        int *demodulated_bits = malloc(bit_length * sizeof(int));
        if (!demodulated_bits) {
            fprintf(stderr, "Memory allocation failed for demodulated bits\n");
            free(signal);
            exit(EXIT_FAILURE);
        }

        demodulate_bpsk(signal, fs, f0, baud, length, demodulated_bits, &bit_length);
        binary_to_text(demodulated_bits, bit_length, output_file);

        free(signal);
        free(demodulated_bits);

        printf("Demodulation complete. Recovered text saved to %s\n", output_file);
    }

    return 0;
}
