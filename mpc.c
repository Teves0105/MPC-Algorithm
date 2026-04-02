#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Declaration constant and variables
#define ALPHA 1.0   
#define BETA 0.15    
#define GAMMA 1500  
#define SEGMENT_DURATION 2.0

int AVAILABLE_BITRATES[] = { 480, 720, 1080, 2000, 3000 }; // kps
#define NUM_BITRATES 5
#define LOOKAHEAD_STEPS 3 // Early Prediction 3 Steps for all possible direction

// Compute QoE for each jump
double calculate_step_qoe(int current_bitrate, int next_bitrate, double current_buffer, double estimated_bw, double* next_buffer) {
    
    double size_kb = next_bitrate * SEGMENT_DURATION;

    double t_download = size_kb / estimated_bw;

    *next_buffer = fmax(0.0, current_buffer - t_download) + SEGMENT_DURATION;

    double rebuffer = fmax(0.0, current_buffer - t_download);

    double qoe = ALPHA*next_bitrate - BETA*abs(next_bitrate - current_bitrate) - GAMMA*rebuffer;

    return qoe;
}

double max_qoe_global = -1e9;
int best_next_bitrate = 480;

// Implement DFS for MPC - Using template of DFS to setting up
void mpc_DFS(int step, double current_buffer, int last_bitrate, double accumulated_qoe, int first_step_decision, double estimated_bw) {
    
    if (step == LOOKAHEAD_STEPS) {
        if (accumulated_qoe > max_qoe_global) {
            max_qoe_global = accumulated_qoe;
            best_next_bitrate = first_step_decision;
        }
        return;
    }

    for (int i = 0; i < NUM_BITRATES; i++) {
        int candidate_bitrate = AVAILABLE_BITRATES[i];
        double next_buffer;

        double step_qoe = calculate_step_qoe(last_bitrate, candidate_bitrate, current_buffer, estimated_bw, &next_buffer);

        int decision = (step == 0) ? candidate_bitrate : first_step_decision;

        mpc_DFS(step + 1, next_buffer, candidate_bitrate, accumulated_qoe + step_qoe, decision, estimated_bw);
    }
}
int main() {
    printf("MPC Algorithm\n");
    double current_buffer = 0.0;
    int current_bitrate = AVAILABLE_BITRATES[0];

    double actual_bandwidths[] = { 4000.0, 3500.0, 800.0, 400.0, 450.0, 1200.0, 2200.0, 4000.0 };
    int total_segments = 8;

    for (int seg = 0; seg < total_segments; seg++) {
        printf("Current [Segment %d] Buffer: %.2fs\n", seg + 1, current_buffer);

        double estimated_bw = actual_bandwidths[seg];

        max_qoe_global = -1e9;

        mpc_DFS(0, current_buffer, current_bitrate, 0.0, 0, estimated_bw);

        printf("  -> [Network Prediction: %.0f kbps] MPC decide to choose: %d kbps\n", estimated_bw, best_next_bitrate);

        double actual_download_time = (best_next_bitrate * SEGMENT_DURATION) / actual_bandwidths[seg];

        if (actual_download_time > current_buffer) {
            printf("  -> [WARNING] Out of Rebuffer %.2fs!\n", actual_download_time - current_buffer);
            current_buffer = 0.0; 
        }
        else {
            current_buffer -= actual_download_time; 
        }

        current_buffer += SEGMENT_DURATION; 
        current_bitrate = best_next_bitrate; 

        printf("  -> Installed %.2fs. Updated Buffer : %.2fs\n\n", actual_download_time, current_buffer);
    }
    return 0;
}