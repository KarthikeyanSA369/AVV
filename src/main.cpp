/**
 * @file main.cpp
 * @brief Entry point for the MyPlayer media player application.
 *
 * Parses command-line arguments, initializes the player, loads media,
 * and runs the main event loop.
 */

#include "player.h"
#include <iostream>
#include <string>

/**
 * @brief Print usage information to stdout.
 */
void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <media_file>" << std::endl;
    std::cout << std::endl;
    std::cout << "Supported formats:" << std::endl;
    std::cout << "  Video: MP4, MKV, AVI, MOV, HEVC, H264, AV1" << std::endl;
    std::cout << "  Audio: MP3, WAV, FLAC, AAC" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Space       - Play / Pause" << std::endl;
    std::cout << "  S           - Stop" << std::endl;
    std::cout << "  F           - Toggle fullscreen" << std::endl;
    std::cout << "  ESC         - Exit (or exit fullscreen)" << std::endl;
    std::cout << "  Up/Down     - Volume +/-" << std::endl;
    std::cout << "  Left/Right  - Seek -10s / +10s" << std::endl;
    std::cout << "  Mouse       - Click seek bar to seek" << std::endl;
}

/**
 * @brief Application entry point.
 * @param argc Argument count.
 * @param argv Argument values.
 * @return Exit code (0 on success, non-zero on failure).
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    const std::string mediaFile = argv[1];

    // Create and initialize the player.
    myplayer::Player player;
    if (!player.Initialize("MyPlayer - Professional Media Player", 1280, 720)) {
        std::cerr << "Failed to initialize player." << std::endl;
        return 1;
    }

    // Load the media file.
    if (!player.LoadMedia(mediaFile)) {
        std::cerr << "Failed to load media file: " << mediaFile << std::endl;
        return 1;
    }

    std::cout << "Loaded: " << mediaFile << std::endl;
    std::cout << "Duration: " << player.GetDuration() << " seconds" << std::endl;
    std::cout << "Press Space to play, ESC to quit." << std::endl;

    // Start playback immediately.
    player.Play();

    // Run the main event loop.
    return player.RunEventLoop();
}