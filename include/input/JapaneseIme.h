#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

class JapaneseIme
{
  public:
    struct Entry {
        std::string_view reading;
        std::string_view candidates;
    };

    static constexpr uint32_t toggleKey = 0x110000;
    static constexpr size_t maxReadingKeys = 220;

    explicit JapaneseIme(std::span<const Entry> dictionary) : dictionary(dictionary) {}

    bool accepts(char key) const;
    bool append(char key);
    void cycleCandidate();
    void backspace();
    void clear();
    bool composing() const { return !keys.empty(); }
    size_t candidateNumber() const { return candidateIndex; }
    size_t candidateCount() const { return choices.empty() ? 0 : dictionaryCandidateCount + 1; }
    std::string text(bool final = false) const;

  private:
    std::span<const Entry> dictionary;
    std::string keys;
    std::string_view choices;
    size_t candidateIndex = 0;
    size_t dictionaryCandidateCount = 0;
};
