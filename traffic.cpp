#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>

using namespace std;

struct TrafficData {
    string timestamp;
    string traffic_light_id;
    int num_cars;
};

class BoundedBuffer {
private:
    queue<TrafficData> buffer;
    const int capacity; 
    mutex mtx; 
    condition_variable not_empty;
    condition_variable not_full; 

public:
    BoundedBuffer(int capacity) : capacity(capacity) {} 

    void add(const TrafficData &data); 
    TrafficData remove(); 
};

void producer(const string &filename, BoundedBuffer &buffer);
void consumer(BoundedBuffer &buffer);

void BoundedBuffer::add(const TrafficData &data) {
    unique_lock<mutex> lock(mtx); 
    not_full.wait(lock, [this]() { return buffer.size() < capacity; });
    buffer.push(data); 
    not_empty.notify_one(); 
}

TrafficData BoundedBuffer::remove() {
    unique_lock<mutex> lock(mtx);
    not_empty.wait(lock, [this]() { return !buffer.empty(); });
    TrafficData data = buffer.front(); 
    buffer.pop(); 
    not_full.notify_one(); 
    return data; 
}

void producer(const string &filename, BoundedBuffer &buffer) {
    ifstream input_file(filename); 
    if (!input_file.is_open()) {
        cerr << "Error opening file: " << filename << endl; 
        return;
    }

    string line;
    while (getline(input_file, line)) {
        istringstream iss(line); 
        string timestamp, traffic_light_id;
        int num_cars;
        char comma;
        iss >> timestamp >> traffic_light_id >> comma >> num_cars; 
        TrafficData data{timestamp, traffic_light_id, num_cars}; 
        buffer.add(data); 
    }
}

void consumer(BoundedBuffer &buffer) {
    vector<TrafficData> traffic_data; 
    string current_hour = ""; 
    while (true) {
        TrafficData data = buffer.remove(); 

        string hour = data.timestamp.substr(0, 2); 

        if (hour != current_hour && !traffic_data.empty()) {
            cout << "Time: " << current_hour <<":00"<< "\n\n"; 
            sort(traffic_data.begin(), traffic_data.end(), [](const TrafficData &a, const TrafficData &b)
                      { return a.num_cars < b.num_cars; }); 

            TrafficData most_congested_light = traffic_data.back(); 

            cout << "Most Congested Traffic Light:\n";
            cout << "1. Traffic Light ID: " << most_congested_light.traffic_light_id << "\n";
            cout << "2. Number of Cars: " << most_congested_light.num_cars << "\n";
            cout << "\n";

            traffic_data.clear(); 
        }

        traffic_data.push_back(data); 
        current_hour = hour; 
    }
}

int main() {
    const int buffer_size = 50; 
    
    BoundedBuffer buffer(buffer_size); 

    thread producer_thread(producer, "datafile.txt", ref(buffer)); 
    thread consumer_thread(consumer, ref(buffer)); 

    producer_thread.join(); 
    consumer_thread.join(); 

    return 0;
}