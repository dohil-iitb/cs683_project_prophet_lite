#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <algorithm>

using namespace std;

// each entry structure
struct PrefetchHint {
    long long pc;
    long long target_address;
    long long delta_cycles;
    long long frequency;
    double confidence;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <trace_file>" << endl;
        return 1;
    }
    
    string filename = argv[1];
    
    // Opening the trace file
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cout << "Error: Cannot open file" << endl;
        return 1;
    }
    
    cout << "Loading ChampSim trace: " << filename << endl;
    
    // transitions[(pc, address)] = {count, sum_delta}
    map<pair<long long, long long>, pair<long long, long long>> transitions;
    
    // last_access[pc] = {address, cycle}
    map<long long, pair<long long, long long>> last_access;
    
    long long total_accesses = 0;
    long long total_transitions = 0;
    long long cycle = 0;
    
    // Read binary trace file
    long long value;
    while (file.read((char*)&value, sizeof(long long))) {
        long long address = value & 0xFFFFFFFFFF;  // Lower 40 bits = address
        long long pc = (value >> 40) & 0xFFFF;      // Bits 40-55 = PC
        
        total_accesses++;
        
        // If this PC accessed memory before, record the transition
        if (last_access.count(pc) > 0) {
            long long prev_addr = last_access[pc].first;
            long long prev_cycle = last_access[pc].second;
            long long delta = cycle - prev_cycle;
            
            // Dont count if it occured too far away else it would get replaced anyways
            if (delta > 0 && delta < 100000) {
                pair<long long, long long> key = make_pair(pc, address);
                

                if (transitions.count(key) == 0) {
                    transitions[key] = make_pair(0, 0);
                }
                
                transitions[key].first++;
                transitions[key].second += delta;
                total_transitions++;
            }
        }
        
        // Update last access for this PC
        last_access[pc] = make_pair(address, cycle);
        cycle++;
        
    }
    
    file.close();
    
    cout << "\ntotal_accesses= " << total_accesses;
    cout << "\ntotal_transitions= " << total_transitions;
    cout << "\ntransitions stored= " << transitions.size();
    
    // Generate hints
    vector<PrefetchHint> hints;
    
    for (auto& entry : transitions) {
        long long pc = entry.first.first;
        long long address = entry.first.second;
        long long count = entry.second.first;
        long long sum_delta = entry.second.second;
        
        if (count >= 2) {  // Only keep patterns that repeat at least twice
            long long avg_delta = sum_delta / count;
            double conf = (double)count / 10.0;
            if (conf > 1.0) conf = 1.0;
            
            PrefetchHint hint;
            hint.pc = pc;
            hint.target_address = address;
            hint.delta_cycles = avg_delta;
            hint.frequency = count;
            hint.confidence = conf;
            
            hints.push_back(hint);
        }
    }
    
    sort(hints.begin(), hints.end(), [](const PrefetchHint& a, const PrefetchHint& b) {
        return a.frequency > b.frequency;
    });
    
    
    // creating the hints file
    string save_location = "hints_"+ filename+".txt";
    ofstream hints_file(save_location);
    if (hints_file.is_open()) {
        for (auto& h : hints) {
            hints_file << hex << h.pc << " " << h.target_address << " " 
                       << dec << h.delta_cycles << " " << h.frequency << " " 
                       << fixed << setprecision(3) << h.confidence << "\n";
        }
        hints_file.close();
        cout << "\nHints saved to: "<< save_location << endl;
    }
    
    return 0;
}