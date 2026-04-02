# MPC Algorithm for Adaptive Bitrate Streaming

## Description

This project implements a Model Predictive Control (MPC) algorithm for adaptive bitrate selection in video streaming. The algorithm aims to optimize Quality of Experience (QoE) by predicting future network conditions and selecting the best bitrate for video segments over a lookahead horizon. It uses a depth-first search (DFS) to explore possible bitrate sequences and chooses the one that maximizes cumulative QoE.

The implementation is written in C and simulates the bitrate adaptation process over multiple video segments with varying network bandwidth conditions.

## Features

- **QoE Optimization**: Balances video quality, bitrate changes, and rebuffering penalties using a configurable QoE metric.
- **Lookahead Prediction**: Considers future segments (configurable steps) to make informed decisions.
- **Configurable Parameters**: Easily adjustable constants for QoE weights, segment duration, and available bitrates.
- **Simulation Mode**: Includes a simulation with predefined bandwidth traces to demonstrate the algorithm's behavior.

## Algorithm Overview

The MPC algorithm works as follows:

1. **QoE Calculation**: For each potential bitrate transition, compute the QoE using the formula:
   ```
   QoE = α × bitrate - β × |bitrate_change| - γ × rebuffer_time
   ```
   Where:
   - `α` (ALPHA): Weight for video quality
   - `β` (BETA): Penalty for bitrate changes
   - `γ` (GAMMA): Penalty for rebuffering

2. **Lookahead Search**: Use DFS to explore all possible bitrate sequences over a fixed number of future segments (LOOKAHEAD_STEPS).

3. **Decision Making**: Select the bitrate for the next segment that leads to the highest cumulative QoE in the lookahead window.

4. **Buffer Management**: Update the playback buffer based on download times and segment duration.

## Requirements

- C compiler (e.g., GCC)
- Standard C libraries: `stdio.h`, `stdlib.h`, `math.h`

## Compilation and Usage

### Compilation

Compile the program using GCC:

```bash
gcc -o mpc mpc.c -lm
```

The `-lm` flag is required to link the math library for functions like `fmax` and `abs`.

### Usage

Run the compiled executable:

```bash
./mpc
```

The program will simulate the MPC algorithm over 8 video segments with predefined bandwidth conditions and print the decisions and buffer states for each segment.

## Example Output

```
MPC Algorithm
Current [Segment 1] Buffer: 0.00s
  -> [Network Prediction: 4000 kbps] MPC decide to choose: 3000 kbps
  -> Installed 1.50s. Updated Buffer : 0.50s

Current [Segment 2] Buffer: 0.50s
  -> [Network Prediction: 3500 kbps] MPC decide to choose: 3000 kbps
  -> Installed 1.71s. Updated Buffer : 0.79s

...
```

## Configuration

Key parameters can be modified in the source code:

- `ALPHA`, `BETA`, `GAMMA`: QoE weights
- `SEGMENT_DURATION`: Duration of each video segment in seconds
- `AVAILABLE_BITRATES`: Array of available bitrate options
- `LOOKAHEAD_STEPS`: Number of future segments to consider
- Bandwidth array in `main()`: Simulated network conditions

## Limitations

- This is a simplified simulation and does not include real-time network monitoring or actual video playback.
- The DFS approach may not scale well for larger lookahead windows or more bitrate options.
- Assumes perfect bandwidth prediction; in practice, prediction accuracy would affect performance.

## Contributing

Feel free to submit issues or pull requests for improvements, bug fixes, or extensions.

## License

This project is provided as-is for educational and research purposes.