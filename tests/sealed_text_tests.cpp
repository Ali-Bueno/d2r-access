#include "sealed_text.h"
#include "test_support.h"

using namespace D2RAccess;

int main() {
	const auto key = SealedText::NewKey();
	Check(key.has_value(), "a key is made");
	const SealedText::Texts texts{{"enUS", "Blood Moor [2] = A moor.\n"}, {"jaJP", "\xE8\x8D\x92\xE9\x87\x8E [2] = \xE6\xB2\xBC\xE3\x80\x82\n"}};
	const auto sealed = SealedText::Seal(SealedText::Pack(texts), *key);
	Check(sealed.has_value(), "the texts are sealed");

	const auto opened = SealedText::Open(*sealed, *key);
	Check(opened.has_value() && SealedText::Unpack(*opened) == texts, "and open again unchanged, in any script");
	Check(SealedText::Seal(SealedText::Pack(texts), *key) != sealed, "each seal has its own nonce");

	auto altered = *sealed;
	altered[altered.size() / 2] ^= 1;
	Check(!SealedText::Open(altered, *key), "one altered byte and it does not open");
	auto cut = *sealed;
	cut.pop_back();
	Check(!SealedText::Open(cut, *key), "nor when cut short");
	auto otherKey = *key;
	otherKey[0] ^= 1;
	Check(!SealedText::Open(*sealed, otherKey), "nor under another key");
	Check(!SealedText::Open({}, *key), "nor from nothing");

	Check(SealedText::Unpack(SealedText::Pack({})).empty(), "nothing packs to nothing");
	Check(SealedText::Unpack("no mark at the start").empty(), "text that was not packed holds no texts");
	Passed();
	return 0;
}
