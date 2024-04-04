#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <condition_variable>

// Define the structure for traffic signal data
struct TrafficData {
    std::time_t timestamp; // Timestamp of when the data was recorded
    int trafficLightId; // Identifier for the traffic light
    int numCarsPassed; // Number of cars passed the traffic light

    // Constructor with default values
    TrafficData(std::time_t t = 0, int id = 0, int numCars = 0)
        : timestamp(t), trafficLightId(id), numCarsPassed(numCars) {}
};

// Comparator for priority queue to sort TrafficData based on numCarsPassed
struct TrafficDataComparator {
    bool operator()(const TrafficData& a, const TrafficData& b) {
        return a.numCarsPassed < b.numCarsPassed;
    }
};

// Bounded buffer class for thread-safe access
template<typename T>
class BoundedBuffer
{
private:
    std::queue<T> buffer; // Queue to store data
    const size_t capacity; // Maximum capacity of the buffer
    std::mutex mtx; // Mutex for thread safety
    std::condition_variable cv; // Condition variable for synchronization

public:
    BoundedBuffer(size_t cap) : capacity(cap) {}

    // Add an item to the buffer
    void add(const T& item)
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return buffer.size() < capacity; });
        buffer.push(item);
        cv.notify_all();
    }

    // Remove an item from the buffer
    T remove()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !buffer.empty(); });
        T item = buffer.front();
        buffer.pop();
        cv.notify_all();
        return item;
    }

    // Check if the buffer is empty
    bool isEmpty()
    {
        std::unique_lock<std::mutex> lock(mtx);
        return buffer.empty();
    }
};

// Producer function to read traffic data from file and add to the bounded buffer
void producer(BoundedBuffer<TrafficData>& buffer, const std::string& filename) {
    try {
        std::ifstream file(filename);
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string timestampStr, trafficLightIdStr, numCarsPassedStr;
            if (!(iss >> timestampStr >> trafficLightIdStr >> numCarsPassedStr)) { 
                continue; 
            }

            // Parse timestamp string into a std::time_t object
            std::tm tm = {};
            std::istringstream timestampStream(timestampStr);
            timestampStream >> std::get_time(&tm, "%H:%M:%S");
            std::time_t timestamp = std::mktime(&tm);

            int trafficLightId = std::stoi(trafficLightIdStr);
            int numCarsPassed = std::stoi(numCarsPassedStr);
            buffer.add(TrafficData(timestamp, trafficLightId, numCarsPassed));
            std::this_thread::sleep_for(std::chrono::minutes(5)); // Wait for 5 minutes
        }
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "Exception in producer: " << e.what() << std::endl;
    }
}

// Consumer function to process traffic data and maintain top N congested traffic lights
void consumer(BoundedBuffer<TrafficData>& buffer, int topN) {
    std::priority_queue<TrafficData, std::vector<TrafficData>, TrafficDataComparator> congestedLights;
    while (true) {
        try {
            // Wait for an hour
            std::this_thread::sleep_for(std::chrono::hours(1));

            // Process data of the past hour
            std::vector<TrafficData> hourData;
            while (!buffer.isEmpty()) {
                hourData.push_back(buffer.remove());
            }

            // Sort the data by numCarsPassed
            std::sort(hourData.begin(), hourData.end(), [](const TrafficData& a, const TrafficData& b) {
                return a.numCarsPassed > b.numCarsPassed; // Sort in descending order
            });

            // Get the top N congested traffic lights
            for (int i = 0; i < std::min(topN, static_cast<int>(hourData.size())); ++i) {
                congestedLights.push(hourData[i]);
            }

            // Print the top N congested traffic lights
            std::cout << "Top " << topN << " congested traffic lights in the past hour:\n";
            for (int i = 0; i < topN && !congestedLights.empty(); ++i) {
                TrafficData data = congestedLights.top();
                congestedLights.pop();
                std::cout << "Timestamp: " << std::ctime(&data.timestamp) << "Traffic Light ID: " << data.trafficLightId << ", Cars Passed: " << data.numCarsPassed << std::endl;
            }
            std::cout << "------------------------\n";
        } catch (const std::exception& e) {
            std::cerr << "Exception in consumer: " << e.what() << std::endl;
        }
    }
}

int main() {
    try {
        const int topN = 3; // Top N congested traffic lights

        BoundedBuffer<TrafficData> buffer(100); // Bounded buffer with capacity 100

        // Create producer thread
        std::thread producerThread(producer, std::ref(buffer), "TrafficDataFile.txt");

        // Create consumer thread
        std::thread consumerThread(consumer, std::ref(buffer), topN);

        // Join producer thread
        if (producerThread.joinable()) {
            producerThread.join();
        }

        // Join consumer thread
        if (consumerThread.joinable()) {
            consumerThread.join();
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
