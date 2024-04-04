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
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct TrafficRecord {
    std::string time;
    std::string light_id;
    int car_count;
};

class TrafficBuffer {
private:
    const int capacity;
    std::mutex mtx;
    std::condition_variable not_empty;
    std::condition_variable not_full;

public:
    std::queue<TrafficRecord> data_buffer;

    explicit TrafficBuffer(int capacity) : capacity(capacity) {}

    void add(const TrafficRecord &record);
    TrafficRecord remove();
};

void trafficProducer(const std::string &filename, TrafficBuffer &buffer);
void trafficConsumer(TrafficBuffer &buffer);

void TrafficBuffer::add(const TrafficRecord &record) {
    std::unique_lock<std::mutex> lock(mtx);
    not_full.wait(lock, [this]() { return static_cast<int>(data_buffer.size()) < capacity; });
    data_buffer.push(record);
    not_empty.notify_one();
}

TrafficRecord TrafficBuffer::remove() {
    std::unique_lock<std::mutex> lock(mtx);
    not_empty.wait(lock, [this]() { return !data_buffer.empty(); });
    TrafficRecord record = data_buffer.front();
    data_buffer.pop();
    not_full.notify_one();
    return record;
}

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

void trafficConsumer(TrafficBuffer &buffer) {
    std::vector<TrafficRecord> traffic_records;
    std::string current_hour = "";
    while (true) {
        TrafficRecord record = buffer.remove();
        std::string hour = record.time.substr(0, 2);

        if (hour == "12") break;

        if (hour != current_hour && !traffic_records.empty()) {
            std::cout << "Time: " << current_hour << ":00\n\n";
            std::sort(traffic_records.begin(), traffic_records.end(),
                      [](const TrafficRecord &a, const TrafficRecord &b) { return a.car_count < b.car_count; });

            std::cout << "Max Number Of Cars Crossed Through\n\n";
            // Using auto for the loop variable
            for (auto i = traffic_records.size(); i > traffic_records.size() - 3 && i > 0; --i) {
                const auto &most_congested_light = traffic_records[i - 1];
                std::cout << "Traffic Light ID: " << most_congested_light.light_id << "\n";
                std::cout << "Number Of Cars Passed: " << most_congested_light.car_count << "\n\n";
            }
            std::cout << "--------------------------------------\n\n";
            traffic_records.clear();
        }

        traffic_records.push_back(record);
        current_hour = hour;
    }
}


int main() {
    const int buffer_size = 50;
    TrafficBuffer buffer(buffer_size);

    std::thread producer_thread(trafficProducer, "TrafficDataFile.txt", std::ref(buffer));
    std::thread consumer_thread(trafficConsumer, std::ref(buffer));

    auto start_time = std::chrono::high_resolution_clock::now();

    producer_thread.join();
    consumer_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::cout << "Execution time: " << duration.count() << " microseconds\n";

    return 0;
}
