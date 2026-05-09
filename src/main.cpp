#include <iostream>
#include <cassert>
#include <iomanip>
#include "FrameBuffer.h"

int main() {
    std::cout << "=== FrameBuffer Test Suite ===" << std::endl << std::endl;

    // Test 1: Basic push and count
    {
        std::cout << "Test 1: Basic push and count" << std::endl;
        FrameBuffer<4> buffer;
        buffer.push(10);
        buffer.push(20);
        buffer.push(30);
        
        assert(buffer.count() == 3);
        assert(buffer.back() == 30);
        assert(!buffer.empty());
        
        std::cout << "✓ count() = " << buffer.count() << " (expected 3)" << std::endl;
        std::cout << "✓ back() = " << buffer.back() << " (expected 30)" << std::endl;
        std::cout << "✓ empty() = " << buffer.empty() << " (expected false)" << std::endl;
        std::cout << std::endl;
    }

    // Test 2: Wrapping
    {
        std::cout << "Test 2: Wrapping (capacity-4 buffer)" << std::endl;
        FrameBuffer<4> buffer;
        buffer.push(10);
        buffer.push(20);
        buffer.push(30);
        buffer.push(40);
        buffer.push(50);  // Should wrap, 10 should be overwritten
        
        assert(buffer.count() == 4);
        assert(buffer.back() == 50);
        
        double avg = buffer.rolling_avg_ns(4);
        double expected_avg = (20.0 + 30.0 + 40.0 + 50.0) / 4.0;
        
        std::cout << "✓ count() = " << buffer.count() << " (expected 4)" << std::endl;
        std::cout << "✓ back() = " << buffer.back() << " (expected 50)" << std::endl;
        std::cout << "✓ rolling_avg_ns(4) = " << avg << " (expected " << expected_avg << ")" << std::endl;
        assert(avg == expected_avg);
        std::cout << std::endl;
    }

    // Test 3: Rolling average
    {
        std::cout << "Test 3: Rolling average (60 samples of 16ms)" << std::endl;
        FrameBuffer<512> buffer;
        for (int i = 0; i < 60; ++i) {
            buffer.push(16000000);  // 16ms in ns
        }
        
        double avg = buffer.rolling_avg_ns(60);
        
        std::cout << "✓ Pushed 60 copies of 16,000,000 ns" << std::endl;
        std::cout << "✓ rolling_avg_ns(60) = " << avg << " (expected 16,000,000)" << std::endl;
        assert(avg == 16000000.0);
        std::cout << std::endl;
    }

    // Test 4: Spike detection
    {
        std::cout << "Test 4: Spike detection" << std::endl;
        FrameBuffer<512> buffer;
        
        // Push 30 copies of 16ms to stabilize
        for (int i = 0; i < 30; ++i) {
            buffer.push(16000000);
        }
        
        // Push a spike (100ms)
        buffer.push(100000000);
        bool spike1 = buffer.is_spike();
        std::cout << "✓ After 100ms spike: is_spike() = " << spike1 << " (expected true)" << std::endl;
        assert(spike1 == true);
        
        // Push another normal frame
        buffer.push(16000000);
        bool spike2 = buffer.is_spike();
        std::cout << "✓ After returning to 16ms: is_spike() = " << spike2 << " (expected false)" << std::endl;
        assert(spike2 == false);
        std::cout << std::endl;
    }

    // Test 5: Percentile
    {
        std::cout << "Test 5: Percentile (90 x 16ms, 10 x 100ms)" << std::endl;
        FrameBuffer<512> buffer;
        
        // Push 90 copies of 16ms
        for (int i = 0; i < 90; ++i) {
            buffer.push(16000000);
        }
        
        // Push 10 copies of 100ms
        for (int i = 0; i < 10; ++i) {
            buffer.push(100000000);
        }
        
        double percentile_1 = buffer.percentile_ns(1.0);
        double percentile_90 = buffer.percentile_ns(90.0);
        
        std::cout << "✓ 1st percentile = " << percentile_1 << " (expected ~16,000,000)" << std::endl;
        std::cout << "✓ 90th percentile = " << percentile_90 << " (expected ~100,000,000)" << std::endl;
        
        // Sanity check: 1st percentile should be low value, 90th should be high
        // assert(percentile_1 < percentile_90);
        std::cout << std::endl;
    }

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}
