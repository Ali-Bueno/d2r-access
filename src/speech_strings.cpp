#include "speech_strings.h"

namespace D2RAccess::Strings {

namespace {

const Table* g_current = &English;

}

const Table& Current() {
	return *g_current;
}

void Select(Language language) {
	g_current = language == Language::Spanish ? &Spanish : &English;
}

}
