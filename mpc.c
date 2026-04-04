#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <curl/curl.h>
#include <windows.h> // Required for Sleep() on Windows

// --- 1. SYSTEM CONSTANTS & MPC WEIGHTS ---
#define ALPHA 1.0        // Reward for higher bitrate
#define BETA 0.15        // Penalty for bitrate fluctuations (Smoothness)
#define GAMMA 1500.0     // Heavy penalty for rebuffering (Stall)
#define SEGMENT_DURATION 2.0

int AVAILABLE_BITRATES[] = { 480, 720, 1080, 2000, 3000 }; // kbps
#define NUM_BITRATES 5
#define LOOKAHEAD_STEPS 3 // Number of segments to predict into the future

double max_qoe_global = -1e9;
int best_next_bitrate = 480;

// --- 2. FILE SYSTEM HELPER ---
// This callback function ensures libcurl writes data to disk safely on Windows,
// avoiding CRT boundary issues and file access conflicts.
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

// --- 3. NETWORK MODULE (LIBCURL) ---
double download_segment_and_measure_speed(const char* url, const char* output_filename) {
    CURL *curl;
    CURLcode res;
    curl_off_t speed_bps = 0; 
    double speed_kbps = 0.0;  

    curl = curl_easy_init();
    if(curl) {
        FILE *fp = fopen(output_filename, "wb");
        if (!fp) {
            printf("  [Error] Failed to open file for writing: %s\n", output_filename);
            return 1.0; 
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        // FIX: Use Write Callback instead of letting libcurl handle file pointers internally
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // 5 seconds connection timeout

        // Execute the HTTP request
        res = curl_easy_perform(curl);

        // Flush and close file immediately to release OS file locks
        fflush(fp);
        fclose(fp);

        if(res == CURLE_OK) {
            // Retrieve average download speed in Bytes/s
            curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD_T, &speed_bps);
            // Convert to kilobits per second (kbps)
            speed_kbps = (speed_bps * 8.0) / 1000.0;
        } else {
            fprintf(stderr, "  [Error] Libcurl execution failed: %s\n", curl_easy_strerror(res));
            speed_kbps = 1.0; // Return minimal speed to signal error
        }

        curl_easy_cleanup(curl);
    }
    return speed_kbps;
}

// --- 4. MPC LOGIC MODULE ---
double calculate_step_qoe(int current_bitrate, int next_bitrate, double current_buffer, double estimated_bw, double* next_buffer) {
    double size_kb = next_bitrate * SEGMENT_DURATION;
    double t_download = size_kb / estimated_bw;
    *next_buffer = fmax(0.0, current_buffer - t_download) + SEGMENT_DURATION;
    double rebuffer = fmax(0.0, t_download - current_buffer);
    double qoe = ALPHA * next_bitrate - BETA * abs(next_bitrate - current_bitrate) - GAMMA * rebuffer;
    return qoe;
}

// DFS algorithm to explore the future bitrate tree and find the optimal path
void mpc_DFS(int step, double current_buffer, int last_bitrate, double accumulated_qoe, int first_step_decision, double estimated_bw) {
    if (step == LOOKAHEAD_STEPS) {
        if (accumulated_qoe > max_qoe_global) {
            max_qoe_global = accumulated_qoe;
            best_next_bitrate = first_step_decision; // Store the best immediate action
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

// --- 5. MAIN EXECUTION LOOP ---
int main() {
    printf("=== DASH MPC PLAYER (Stable Version) ===\n\n");
    
    double current_buffer = 0.0;
    int current_bitrate = AVAILABLE_BITRATES[0];
    double estimated_bw = 4000.0; // Initial bandwidth estimate
    int total_segments = 8;

    for (int seg = 0; seg < total_segments; seg++) {
        printf("Current State [Segment %d] | Buffer: %.2fs\n", seg + 1, current_buffer);

        // 1. Run MPC Optimization to choose best bitrate
        max_qoe_global = -1e9;
        mpc_DFS(0, current_buffer, current_bitrate, 0.0, 0, estimated_bw);
        printf("  -> MPC Decision: %d kbps (Estimated BW: %.0f kbps)\n", best_next_bitrate, estimated_bw);

        // 2. Prepare HTTP Request
        char url[256];
        char local_file[256];
        sprintf(url, "http://127.0.0.1:8000/my_video_seg%d_%dkbps.mp4", seg, best_next_bitrate);
        sprintf(local_file, "temp_seg_%d.mp4", seg); 

        // 3. Perform Download and Measure Actual Bandwidth
        printf("  -> Downloading: %s\n", url);
        double actual_bw_measured = download_segment_and_measure_speed(url, local_file);
        
        // Connectivity check
        if (actual_bw_measured <= 10.0) {
            printf("  [Critical] Download failed! Ensure Python Server is running.\n");
            estimated_bw = 100.0; // Assume bad network for next segment
            continue; 
        }

        // Use current measurement as estimation for the next segment
        estimated_bw = actual_bw_measured; 
        double actual_download_time = (best_next_bitrate * SEGMENT_DURATION) / actual_bw_measured;

        // 4. Update Playback Buffer State
        if (actual_download_time > current_buffer) {
            printf("  -> [STALL] Video paused. Rebuffering for %.2fs\n", actual_download_time - current_buffer);
            current_buffer = 0.0;
        } else {
            current_buffer -= actual_download_time;
        }
        current_buffer += SEGMENT_DURATION; // Add the newly downloaded 2s segment
        current_bitrate = best_next_bitrate;

        // 5. Finalize file and Launch Player
        printf("  -> Download successful! Finalizing file sync...\n");
        Sleep(500); // Wait 0.5s to ensure Windows finishes disk write operations

        char play_command[256];
        sprintf(play_command, "ffplay -autoexit -x 800 -y 450 -v warning %s", local_file);
        
        printf("  -> [Player] Launching Segment %d on-screen...\n\n", seg);
        system(play_command);
    }

    printf("=== VIDEO PLAYBACK FINISHED ===\n");
    return 0;
}