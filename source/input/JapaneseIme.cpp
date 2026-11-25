#if defined(MESHTASTIC_DEVICE_UI_JAPANESE_IME) || defined(UNIT_TEST)

#include "input/JapaneseIme.h"
#include <algorithm>

namespace
{
struct KanaRule {
    std::string_view romaji;
    std::string_view kana;
};

constexpr KanaRule rules[] = {
    {"kya", "きゃ"}, {"kyu", "きゅ"}, {"kyo", "きょ"}, {"gya", "ぎゃ"}, {"gyu", "ぎゅ"}, {"gyo", "ぎょ"}, {"sha", "しゃ"},
    {"shu", "しゅ"}, {"sho", "しょ"}, {"sya", "しゃ"}, {"syu", "しゅ"}, {"syo", "しょ"}, {"cha", "ちゃ"}, {"chu", "ちゅ"},
    {"cho", "ちょ"}, {"tya", "ちゃ"}, {"tyu", "ちゅ"}, {"tyo", "ちょ"}, {"cya", "ちゃ"}, {"cyu", "ちゅ"}, {"cyo", "ちょ"},
    {"jya", "じゃ"}, {"jyu", "じゅ"}, {"jyo", "じょ"}, {"zya", "じゃ"}, {"zyu", "じゅ"}, {"zyo", "じょ"}, {"dya", "ぢゃ"},
    {"dyu", "ぢゅ"}, {"dyo", "ぢょ"}, {"nya", "にゃ"}, {"nyu", "にゅ"}, {"nyo", "にょ"}, {"hya", "ひゃ"}, {"hyu", "ひゅ"},
    {"hyo", "ひょ"}, {"bya", "びゃ"}, {"byu", "びゅ"}, {"byo", "びょ"}, {"pya", "ぴゃ"}, {"pyu", "ぴゅ"}, {"pyo", "ぴょ"},
    {"mya", "みゃ"}, {"myu", "みゅ"}, {"myo", "みょ"}, {"rya", "りゃ"}, {"ryu", "りゅ"}, {"ryo", "りょ"}, {"she", "しぇ"},
    {"che", "ちぇ"}, {"thi", "てぃ"}, {"dhi", "でぃ"}, {"twu", "とぅ"}, {"dwu", "どぅ"}, {"shi", "し"},   {"chi", "ち"},
    {"tsu", "つ"},   {"xtsu", "っ"},  {"ltsu", "っ"},  {"xtu", "っ"},   {"ltu", "っ"},   {"xya", "ゃ"},   {"xyu", "ゅ"},
    {"xyo", "ょ"},   {"lya", "ゃ"},   {"lyu", "ゅ"},   {"lyo", "ょ"},   {"ka", "か"},    {"ki", "き"},    {"ku", "く"},
    {"ke", "け"},    {"ko", "こ"},    {"ga", "が"},    {"gi", "ぎ"},    {"gu", "ぐ"},    {"ge", "げ"},    {"go", "ご"},
    {"sa", "さ"},    {"si", "し"},    {"su", "す"},    {"se", "せ"},    {"so", "そ"},    {"za", "ざ"},    {"zi", "じ"},
    {"zu", "ず"},    {"ze", "ぜ"},    {"zo", "ぞ"},    {"ta", "た"},    {"ti", "ち"},    {"tu", "つ"},    {"te", "て"},
    {"to", "と"},    {"da", "だ"},    {"di", "ぢ"},    {"du", "づ"},    {"de", "で"},    {"do", "ど"},    {"na", "な"},
    {"ni", "に"},    {"nu", "ぬ"},    {"ne", "ね"},    {"no", "の"},    {"ha", "は"},    {"hi", "ひ"},    {"hu", "ふ"},
    {"he", "へ"},    {"ho", "ほ"},    {"ba", "ば"},    {"bi", "び"},    {"bu", "ぶ"},    {"be", "べ"},    {"bo", "ぼ"},
    {"pa", "ぱ"},    {"pi", "ぴ"},    {"pu", "ぷ"},    {"pe", "ぺ"},    {"po", "ぽ"},    {"ma", "ま"},    {"mi", "み"},
    {"mu", "む"},    {"me", "め"},    {"mo", "も"},    {"ya", "や"},    {"yu", "ゆ"},    {"yo", "よ"},    {"ra", "ら"},
    {"ri", "り"},    {"ru", "る"},    {"re", "れ"},    {"ro", "ろ"},    {"wa", "わ"},    {"wo", "を"},    {"ja", "じゃ"},
    {"ji", "じ"},    {"ju", "じゅ"},  {"je", "じぇ"},  {"jo", "じょ"},  {"fa", "ふぁ"},  {"fi", "ふぃ"},  {"fu", "ふ"},
    {"fe", "ふぇ"},  {"fo", "ふぉ"},  {"va", "ヴァ"},  {"vi", "ヴィ"},  {"vu", "ヴ"},    {"ve", "ヴェ"},  {"vo", "ヴォ"},
    {"wi", "うぃ"},  {"we", "うぇ"},  {"xa", "ぁ"},    {"xi", "ぃ"},    {"xu", "ぅ"},    {"xe", "ぇ"},    {"xo", "ぉ"},
    {"la", "ぁ"},    {"li", "ぃ"},    {"lu", "ぅ"},    {"le", "ぇ"},    {"lo", "ぉ"},    {"a", "あ"},     {"i", "い"},
    {"u", "う"},     {"e", "え"},     {"o", "お"},     {"-", "ー"},
};

constexpr bool startsNasalSyllable(char key)
{
    return std::string_view("aiueoy").find(key) != std::string_view::npos;
}

std::string toKana(std::string_view keys, bool final)
{
    std::string kana;
    while (!keys.empty()) {
        const char first = keys.front();
        const char next = keys.size() > 1 ? keys[1] : '\0';
        if (first == 'n' && !startsNasalSyllable(next) && (next != '\0' || final)) {
            const bool pair = next == '\'' || (next == 'n' && (keys.size() == 2 || !startsNasalSyllable(keys[2])));
            kana += "ん";
            keys.remove_prefix(pair ? 2 : 1);
            continue;
        }
        if ((first == next && std::string_view("bcdfghjkmprstvwz").find(first) != std::string_view::npos) ||
            (first == 't' && keys.substr(1).starts_with("ch"))) {
            kana += "っ";
            keys.remove_prefix(1);
            continue;
        }
        const auto rule = std::find_if(std::begin(rules), std::end(rules),
                                       [keys](const KanaRule &rule) { return keys.starts_with(rule.romaji); });
        if (rule == std::end(rules)) {
            kana += first;
            keys.remove_prefix(1);
        } else {
            kana += rule->kana;
            keys.remove_prefix(rule->romaji.size());
        }
    }
    return kana;
}
} // namespace

bool JapaneseIme::accepts(char key) const
{
    return (key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || key == '-' ||
           (key == '\'' && !candidateIndex && !keys.empty() && keys.back() == 'n');
}

bool JapaneseIme::append(char key)
{
    if (!accepts(key) || keys.size() == maxReadingKeys)
        return false;
    choices = {};
    candidateIndex = dictionaryCandidateCount = 0;
    keys += key >= 'A' && key <= 'Z' ? static_cast<char>(key + ('a' - 'A')) : key;
    return true;
}

void JapaneseIme::cycleCandidate()
{
    if (!choices.empty()) {
        candidateIndex = candidateIndex % candidateCount() + 1;
        return;
    }
    const auto reading = toKana(keys, true);
    const auto entry = std::lower_bound(dictionary.begin(), dictionary.end(), reading,
                                        [](const Entry &entry, std::string_view reading) { return entry.reading < reading; });
    if (entry == dictionary.end() || entry->reading != reading)
        return;
    choices = entry->candidates;
    dictionaryCandidateCount = 1 + std::count(choices.begin(), choices.end(), '\t');
    candidateIndex = 1;
}

void JapaneseIme::backspace()
{
    if (!choices.empty()) {
        choices = {};
        candidateIndex = dictionaryCandidateCount = 0;
    } else if (!keys.empty()) {
        keys.pop_back();
    }
}

void JapaneseIme::clear()
{
    keys.clear();
    choices = {};
    candidateIndex = dictionaryCandidateCount = 0;
}

std::string JapaneseIme::text(bool final) const
{
    if (candidateIndex == 0 || candidateIndex > dictionaryCandidateCount)
        return toKana(keys, final || candidateIndex != 0);

    auto candidate = choices;
    for (size_t index = 1; index < candidateIndex; ++index)
        candidate.remove_prefix(candidate.find('\t') + 1);
    return std::string(candidate.substr(0, candidate.find('\t')));
}

#endif
