#pragma once

#include <span>
#include <string>
#include <string_view>

// What the game tells a sighted player about a monster beyond its base name,
// turned into words to speak. Pure, so it is unit-tested without the game.
namespace D2RAccess::MonsterPlate {
// One of a phrase's grammatical forms: "[ms]fanatischer[fs]fanatische" holds
// one per gender tag. The form for `tag` ("[fs]") when the phrase has it,
// otherwise its first; a phrase with no tags comes back whole.
std::string Form(std::string_view text, std::string_view tag = {});
// A name the game composes from a format and its parts, as its name builder
// (0x1400985A0) does: a champion's "%0 %1" (its kind, then the base name), a
// random unique's "%0 %1" or "%0 %1 %2". A format opening "a<i>n<j>:" makes
// part i an adjective agreeing with part j, the noun, by the noun's tag.
std::string Compose(std::string_view format, std::span<const std::string> parts);
}
