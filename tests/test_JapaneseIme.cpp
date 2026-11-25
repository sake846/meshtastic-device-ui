#include "input/JapaneseIme.h"
#include <doctest/doctest.h>

namespace
{
constexpr JapaneseIme::Entry dictionary[] = {{"かんじ", "漢字\t幹事"}, {"きょう", "今日"}};

void type(JapaneseIme &ime, std::string_view text)
{
    for (char key : text)
        REQUIRE(ime.append(key));
}
} // namespace

TEST_CASE("Japanese IME composes romaji without losing incomplete keystrokes")
{
    JapaneseIme ime(dictionary);
    type(ime, "Konnichiha");
    CHECK(ime.text() == "こんにちは");
    ime.clear();
    type(ime, "gakkou");
    CHECK(ime.text() == "がっこう");
    ime.clear();
    type(ime, "shin'you");
    CHECK(ime.text() == "しんよう");
    ime.clear();
    type(ime, "pan");
    CHECK(ime.text() == "ぱn");
    CHECK(ime.text(true) == "ぱん");
    ime.backspace();
    CHECK(ime.text() == "ぱ");
    ime.clear();
    type(ime, "matcha");
    CHECK(ime.text() == "まっちゃ");
    CHECK_FALSE(ime.append('!'));
    CHECK(ime.text() == "まっちゃ");
}

TEST_CASE("Japanese IME cycles dictionary candidates and allows kana confirmation")
{
    JapaneseIme ime(dictionary);
    type(ime, "kanji");
    CHECK(ime.composing());
    CHECK(ime.text() == "かんじ");
    ime.cycleCandidate();
    CHECK(ime.candidateCount() == 3);
    CHECK(ime.text() == "漢字");
    ime.cycleCandidate();
    CHECK(ime.text() == "幹事");
    ime.cycleCandidate();
    CHECK(ime.text() == "かんじ");
    ime.cycleCandidate();
    CHECK(ime.text(true) == "漢字");
    ime.backspace();
    CHECK(ime.candidateCount() == 0);
    CHECK(ime.text() == "かんじ");
    ime.clear();
    CHECK_FALSE(ime.composing());

    type(ime, "pan");
    ime.cycleCandidate();
    CHECK(ime.candidateCount() == 0);
    CHECK(ime.text(true) == "ぱん");
}

TEST_CASE("Japanese IME bounds a reading without dropping the existing text")
{
    JapaneseIme ime(dictionary);
    type(ime, std::string(JapaneseIme::maxReadingKeys, 'a'));
    CHECK_FALSE(ime.append('i'));
    CHECK(ime.text().size() == 3 * JapaneseIme::maxReadingKeys);
}
