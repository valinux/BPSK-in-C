
---

# BPSK Modulation and Demodulation

This project demonstrates the implementation of Binary Phase Shift Keying (BPSK) for digital data transmission and reception. BPSK is a fundamental digital modulation technique that encodes data by varying the phase of a carrier signal. Despite its simplicity, BPSK is highly versatile and is used in various applications where robustness and simplicity are prioritized over data rate or bandwidth efficiency.

## Features

- **Digital Data Transmission:** Transmit binary data, including text files and encoded digital signals.
- **Communication in Noisy Environments:** Suitable for environments with low signal-to-noise ratio (SNR) such as satellite and military communications.
- **Wireless and Radio Communication:** Applications include RFID systems and amateur radio.
- **Telemetry and Remote Sensing:** Ideal for transmitting telemetry data and environmental monitoring.
- **Simple Control Systems:** Can be used in remote control systems and industrial control.
- **Error-Resilient Applications:** Suitable for low-bitrate communication where reliability is crucial.
- **Education and Research:** Useful for learning and research in digital modulation.
- **Secure Communications:** Can be combined with encryption for secure data transmission.
- **Broadcast Systems:** Applications in digital broadcast systems and public safety communications.

## Advantages of BPSK

- **Simplicity:** Easy to implement, suitable for systems with limited computational resources.
- **Noise Resilience:** Robust to noise and interference.
- **Low Power Consumption:** Efficient for low-power applications.

## Limitations of BPSK

- **Low Data Rate:** Transmits only one bit per symbol.
- **Bandwidth Efficiency:** Less bandwidth-efficient compared to higher-order modulation schemes.

## Typical Applications

- Spacecraft Telemetry
- Military Communication
- Amateur Radio (BPSK31)
- RFID Systems
- Low-Power IoT Devices

## Compilation and Usage

### BPSK Demodulation

To compile the BPSK demodulation program:

```bash
gcc -o DemoBPSK DemoBPSK.c -lm
```

To run the demodulation program:

```bash
./DemoBPSK -i output.wav -o rec.txt -fs 48000 -baud 1200 -f0 2400
```

### BPSK Modulation

To compile the BPSK modulation program:

```bash
gcc -o bpsk BPSKC.c -lm
```

To run the modulation program:

```bash
./bpsk -i input.txt -o output.wav -fs 48000 -baud 1200 -f0 2400
```
