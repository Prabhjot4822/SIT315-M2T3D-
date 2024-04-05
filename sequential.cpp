#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Struct to hold traffic records
struct TrafficRecord {
    std::string time;       // Time of the record
    std::string light_id;   // ID of the traffic light
    int car_count;          // Number of cars
};

// Class for managing a buffer of traffic records
class TrafficBuffer {
private:
    const int capacity;                             // Maximum capacity of the buffer

public:
    std::queue<TrafficRecord> data_buffer;          // Queue to store traffic records

    explicit TrafficBuffer(int capacity) : capacity(capacity) {}

    void add(const TrafficRecord &record);          // Add a record to the buffer
    TrafficRecord remove();                         // Remove a record from the buffer
};

// Function to add a record to the buffer
void TrafficBuffer::add(const TrafficRecord &record) {
    if (static_cast<int>(data_buffer.size()) < capacity) {
        data_buffer.push(record);
    }
}

// Function to remove a record from the buffer
TrafficRecord TrafficBuffer::remove() {
    TrafficRecord record;
    if (!data_buffer.empty()) {
        record = data_buffer.front();
        data_buffer.pop();
    }
    return record;
}

// Function to simulate traffic data production
void trafficProducer(const std::string &filename, TrafficBuffer &buffer) {
    std::ifstream input_file(filename);
    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(input_file, line)) {
        std::istringstream iss(line);
        std::string time, light_id;
        int car_count;
        iss >> time >> light_id >> car_count;
        TrafficRecord record{time, light_id, car_count};
        buffer.add(record);
    }
}

// Function to simulate traffic data consumption and analysis
void trafficConsumer(TrafficBuffer &buffer) {
    std::vector<TrafficRecord> traffic_records;   // Vector to store traffic records
    std::string current_hour = "";                // Track current hour for analysis
    while (true) {
        TrafficRecord record = buffer.remove();   // Get a record from the buffer
        std::string hour = record.time.substr(0, 2);
        if (hour == "12") break;

        // If a new hour is reached, analyze the traffic records of the previous hour
        if (hour != current_hour && !traffic_records.empty()) {
            std::cout << "Time: " << current_hour << ":00\n\n";
            std::sort(traffic_records.begin(), traffic_records.end(),
                      [](const TrafficRecord &a, const TrafficRecord &b) { return a.car_count < b.car_count; });

            std::cout << "Max Number Of Cars Crossed Through\n\n";
            for (auto i = traffic_records.size(); i > traffic_records.size() - 3 && i > 0; --i) {
                const auto &most_congested_light = traffic_records[i - 1];
                std::cout << "Traffic Light ID: " << most_congested_light.light_id << "\n";
                std::cout << "Number Of Cars Passed: " << most_congested_light.car_count << "\n\n";
            }
            std::cout << "--------------------------------------\n\n";
            traffic_records.clear();    // Clear the records for the next hour
        }

        traffic_records.push_back(record);    // Store the current record
        current_hour = hour;                  // Update the current hour
    }
}

int main() {
    const int buffer_size = 100;                 // Maximum size of the buffer
    TrafficBuffer buffer(buffer_size);          // Create a traffic buffer

    // Simulate traffic data production
    trafficProducer("TrafficDataFile.txt", buffer);

    auto start_time = std::chrono::high_resolution_clock::now();   // Record start time

    // Simulate traffic data consumption and analysis
    trafficConsumer(buffer);

    auto end_time = std::chrono::high_resolution_clock::now();     // Record end time
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);   // Calculate duration

    // Output execution time
    std::cout << "Execution time: " << duration.count() << " microseconds\n";

    return 0;
}
