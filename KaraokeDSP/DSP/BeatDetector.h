#ifndef BEATDETECTOR_H
#define BEATDETECTOR_H

#include <vector>
#include <cmath>
#include <numeric>

class BeatDetector {
public:
    BeatDetector(size_t historyCapacity = 43)
        : mHistoryCapacity(historyCapacity) {
    }

    // Dynamic Energy Thresholding for Real-Time Onset Detection
    bool ProcessBuffer(const float* buffer, size_t bufferSize) {
        if (!buffer || bufferSize == 0) return false;

        // 1. Instantaneous Frame Energy
        float instantEnergy = 0.0f;
        for (size_t i = 0; i < bufferSize; ++i) {
            instantEnergy += buffer[i] * buffer[i];
        }

        // 2. Rolling History Average Energy
        float averageEnergy = 0.0f;
        if (!mEnergyHistory.empty()) {
            float sum = std::accumulate(mEnergyHistory.begin(), mEnergyHistory.end(), 0.0f);
            averageEnergy = sum / static_cast<float>(mEnergyHistory.size());
        }

        // 3. Dynamic Beat Check (Threshold Multiplier C = 1.35f)
        const float C = 1.35f;
        bool isBeat = (instantEnergy > C * averageEnergy) && (instantEnergy > 0.001f);

        // 4. Update Rolling History
        mEnergyHistory.push_back(instantEnergy);
        if (mEnergyHistory.size() > mHistoryCapacity) {
            mEnergyHistory.erase(mEnergyHistory.begin());
        }

        return isBeat;
    }

    void Reset() {
        mEnergyHistory.clear();
    }

private:
    size_t mHistoryCapacity;
    std::vector<float> mEnergyHistory;
};

#endif // BEATDETECTOR_H