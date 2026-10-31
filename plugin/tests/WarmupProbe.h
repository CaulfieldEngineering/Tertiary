/* [Claude 2026-10-30, Codex] Optional first-block timing diagnostic. Run TertiaryTests --warmup-probe.
   Construction, prepare, input filling and reporting are outside the processBlock timers. No editor or network.
   Each row is a fresh batch; timings are totals for the batch, not per instance. */
#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

namespace WarmupProbe
{
    using Processor = audio_plugin::AudioPluginAudioProcessor;
    using Clock = std::chrono::steady_clock;

    template <typename Fn>
    double milliseconds (Fn&& fn)
    {
        const auto start = Clock::now();
        fn();
        return std::chrono::duration<double, std::milli> (Clock::now() - start).count();
    }

    inline int run()
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 128, warmBlocks = 100;
        std::cout << std::fixed << std::setprecision (6);

        for (int instances : { 30, 1 })
            for (int repeat = 1; repeat <= 3; ++repeat)
            {
                std::vector<std::unique_ptr<Processor>> processors;
                std::vector<juce::AudioBuffer<float>> buffers;
                juce::MidiBuffer midi;
                processors.reserve ((size_t) instances);
                buffers.reserve ((size_t) instances);
                for (int i = 0; i < instances; ++i)
                {
                    processors.push_back (std::make_unique<Processor>());
                    processors.back()->setDemoState (false);
                    processors.back()->setRateAndBufferSizeDetails (sampleRate, blockSize);
                    processors.back()->prepareToPlay (sampleRate, blockSize);
                    buffers.emplace_back (2, blockSize);
                }

                const auto processBatch = [&]
                {
                    for (auto& buffer : buffers)
                        for (int c = 0; c < buffer.getNumChannels(); ++c)
                            juce::FloatVectorOperations::fill (buffer.getWritePointer (c), 0.25f, blockSize);
                    return milliseconds ([&]
                    {
                        for (int i = 0; i < instances; ++i)
                            processors[(size_t) i]->processBlock (buffers[(size_t) i], midi);
                    });
                };

                const double cold = processBatch();
                double warm = 0.0;
                for (int block = 0; block < warmBlocks; ++block)
                    warm += processBatch();
                for (auto& p : processors)
                {
                    p->releaseResources();
                    p->prepareToPlay (sampleRate, blockSize);
                }
                const double reprepare = processBatch();

                // Empty the preallocated tables as RC1b prepare did. Time each band's exact table builder
                // separately; no processBlock call is made here and no oscillator samples are advanced.
                for (auto& p : processors)
                {
                    p->lowLFO.initializeLFO (sampleRate);
                    p->midLFO.initializeLFO (sampleRate);
                    p->highLFO.initializeLFO (sampleRate);
                }
                const auto tableMs = [&] (LFO Processor::* band)
                {
                    return milliseconds ([&]
                    {
                        for (auto& p : processors)
                            (p.get()->*band).updateLFO (sampleRate, 120.0);
                    });
                };
                const double low = tableMs (&Processor::lowLFO);
                const double mid = tableMs (&Processor::midLFO);
                const double high = tableMs (&Processor::highLFO);

                std::cout << "{\"probe\":\"first_block\",\"instances\":" << instances
                          << ",\"repeat\":" << repeat << ",\"sample_rate\":48000,\"block_size\":128"
                          << ",\"deadline_ms\":" << 1000.0 * blockSize / sampleRate
                          << ",\"cold_batch_ms\":" << cold
                          << ",\"warm_batch_mean_100_ms\":" << warm / warmBlocks
                          << ",\"reprepare_first_batch_ms\":" << reprepare
                          << ",\"low_table_batch_ms\":" << low
                          << ",\"mid_table_batch_ms\":" << mid
                          << ",\"high_table_batch_ms\":" << high
                          << ",\"all_tables_batch_ms\":" << low + mid + high << "}" << std::endl;
            }
        return 0;
    }
}
