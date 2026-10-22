#pragma once
#include "tutorial_text.h"

// Polish (plPL): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts plPL{
	.welcome = {
		"Samouczek D2R Access. Pokażę ci, jak odnajdywać drogę z pomocą moda, aż do ukończenia pierwszego zadania. "
		"Każdy krok prosi cię o zrobienie czegoś; gdy to zrobisz, rozbrzmi dźwięk ukończonego kroku i nadejdzie następny. "
		"{view} i {rs} w lewo lub w prawo przechodzą przez te komunikaty; {view} i {up} wracają do pierwszego. "
		"Samouczek możesz wyłączyć na liście ustawień, przez {view} i {rb}.",
		"Samouczek D2R Access. Pokażę ci, jak odnajdywać drogę z pomocą moda, aż do ukończenia pierwszego zadania. "
		"Każdy krok prosi cię o zrobienie czegoś; gdy to zrobisz, rozbrzmi dźwięk ukończonego kroku i nadejdzie następny. "
		"K i L przechodzą przez te komunikaty; Shift plus K wraca do pierwszego. "
		"Samouczek możesz wyłączyć na liście ustawień, przez Ctrl plus S."},
	.steps = {
		{"Pierwszy krok: twój stan. Naciśnij {view} i {x}, aby usłyszeć swoje zdrowie i manę.",
			"Pierwszy krok: twój stan. Naciśnij Z, aby usłyszeć swoje zdrowie, a X, aby usłyszeć manę."},
		{"Radar brzmi w czterech kierunkach, gdy zmienia się otoczenie; w dół brzmi niżej. Teraz odtworzę jego "
			"dźwięki, każdy po jego nazwie i znaczeniu.",
			"Radar brzmi w czterech kierunkach, gdy zmienia się otoczenie; w dół brzmi niżej. Teraz odtworzę jego "
			"dźwięki, każdy po jego nazwie i znaczeniu."},
		{"Oprócz radaru rzeczy w pobliżu brzmią z miejsca, w którym są, tym głośniej, im są bliżej: ludzie, miejsca i "
			"to, co leży na ziemi, przedmioty z dźwiękami, które wybrałeś na liście ustawień. Dotąd trzymałem je cicho, "
			"żebyś mógł usłyszeć sam radar. Odtworzę każdą po jej nazwie.",
			"Oprócz radaru rzeczy w pobliżu brzmią z miejsca, w którym są, tym głośniej, im są bliżej: ludzie, miejsca i "
			"to, co leży na ziemi, przedmioty z dźwiękami, które wybrałeś na liście ustawień. Dotąd trzymałem je cicho, "
			"żebyś mógł usłyszeć sam radar. Odtworzę każdą po jej nazwie."},
		{"Skaner. Wymienia to, co jest na mapie, według kategorii: wyjścia, NPC, punkty podróży i inne, od najbliższego, z "
			"odległością i kierunkiem. Przy {rs} góra i dół zmieniają kategorię, a lewo i prawo przechodzą przez nią. "
			"Znajdź Warriva w kategorii NPC: to ten, który jest najbliżej ciebie.",
			"Skaner. Wymienia to, co jest na mapie, według kategorii: wyjścia, NPC, punkty podróży i inne, od najbliższego, z "
			"odległością i kierunkiem. Ctrl plus Page Up i Ctrl plus Page Down zmieniają kategorię; Page Up i "
			"Page Down przechodzą przez nią. Znajdź Warriva w kategorii NPC: to ten, który jest najbliżej ciebie."},
		{"Porozmawiaj z Warrivem. Jest kilka kroków stąd: podejdź do niego za pomocą {ls}, tak jak powiedział skaner, naciśnij {a} obok "
			"niego i posłuchaj, co ma ci do powiedzenia. Gdy skończy, zamknij dialog przez {b}.",
			"Porozmawiaj z Warrivem. Jest kilka kroków stąd: podejdź do niego strzałkami, tak jak powiedział skaner, naciśnij "
			"D obok niego i posłuchaj, co ma ci do powiedzenia. Gdy skończy, zamknij dialog przez Esc."},
		{"Teraz Akara, uzdrowicielka obozu: ma pierwsze zadanie. Jest dalej; znajdź ją w kategorii NPC "
			"skanera.",
			"Teraz Akara, uzdrowicielka obozu: ma pierwsze zadanie. Jest dalej; znajdź ją w kategorii NPC "
			"skanera."},
		{"Trasa. Mówi ci, jak dojść do tego, co wskazał skaner, w kierunkach i odległościach, tak jak zaplanowałbyś to "
			"na minimapie; nie idzie za ciebie. Gdy Akara jest wskazana, naciśnij {route}.",
			"Trasa. Mówi ci, jak dojść do tego, co wskazał skaner, w kierunkach i odległościach, tak jak zaplanowałbyś to "
			"na minimapie; nie idzie za ciebie. Gdy Akara jest wskazana, naciśnij Ctrl plus E."},
		{"Prowadzenie. Wysyła sygnały w stronę tego, co wskazał skaner, i prowadzi cię po drodze, "
			"ale idziesz ty. Naciśnij {view} i {lb}, aby podążać za Akarą, i idź w stronę sygnałów, aż do niej dotrzesz; "
			"kolejne naciśnięcie puszcza cel.",
			"Prowadzenie. Wysyła sygnały w stronę tego, co wskazał skaner, i prowadzi cię po drodze, "
			"ale idziesz ty. Naciśnij Ctrl plus B, aby podążać za Akarą, i idź strzałkami w stronę sygnałów, aż do niej dotrzesz; "
			"kolejne naciśnięcie puszcza cel."},
		{"Porozmawiaj z Akarą: obok niej naciśnij {a}. Ma dla ciebie zadanie. "
			"Gdy skończy, zamknij dialog przez {b}.",
			"Porozmawiaj z Akarą: obok niej naciśnij D. Ma dla ciebie zadanie. "
			"Gdy skończy, zamknij dialog przez Esc."},
		{"Poza obozem są potwory, a każdy rodzaj brzmi po swojemu, z miejsca, w którym jest. Zanim wyjdziesz, "
			"odtworzę je, każdy po jego nazwie.",
			"Poza obozem są potwory, a każdy rodzaj brzmi po swojemu, z miejsca, w którym jest. Zanim wyjdziesz, "
			"odtworzę je, każdy po jego nazwie."},
		{"Jesteś w: {moor}. Na zewnątrz zorientują cię dwa pytania: {view} i {b} dają przegląd terenu, a {view} i "
			"{rs} w górę opis obszaru. Wypróbuj przegląd.",
			"Jesteś w: {moor}. Ctrl plus O daje ci przegląd terenu wokół ciebie. Wypróbuj go."},
		{"Są tu potwory: brzmią tak, jak słyszałeś w obozie, a skaner wymienia je w osobnej kategorii. Atakuj przez "
			"{a}; twoje inne umiejętności są na przyciskach, które im przypisałeś. Gdy twoje zdrowie spada, wypij "
			"miksturę przez {dpad}. To, co upuszczą, brzmi z miejsca, w którym leży: podejdź i naciśnij {a}, aby to "
			"podnieść; złoto podnosi się, gdy się po nim przejdzie. Zabij potwora.",
			"Są tu potwory: brzmią tak, jak słyszałeś w obozie, a skaner wymienia je w osobnej kategorii. Tab wybiera "
			"następnego potwora; przytrzymaj A, aby atakować lewą umiejętnością, a W prawą. Gdy twoje zdrowie spada, "
			"mikstury są na klawiszach od 1 do 4. To, co upuszczą, brzmi z miejsca, w którym leży: kropka wybiera "
			"następny przedmiot na ziemi, przecinek poprzedni, a D go podnosi; złoto podnosi się, gdy się po nim "
			"przejdzie. Zabij potwora."},
		{"Znajdź: {den}. Mapa jest tworzona od nowa w każdej grze, więc jej wejście może być blisko lub daleko. "
			"Podążaj za dźwiękami radaru albo za tonami ścian, jeśli masz je włączone: niezbadane przejścia i niezbadana "
			"podłoga prowadzą do nowego terenu, a radar mówi ci, gdy zapisze nowe wyjście, które skaner potem wymienia "
			"wśród swoich wyjść. Szukaj tam: {den}. Gdy znajdzie się na liście, trasa i prowadzenie zaprowadzą cię do "
			"wejścia. Jeśli się zgubisz, kategoria niezbadany pokój skanera pokaże ci, dokąd iść.",
			"Znajdź: {den}. Mapa jest tworzona od nowa w każdej grze, więc jej wejście może być blisko lub daleko. "
			"Podążaj za dźwiękami radaru albo za tonami ścian, jeśli masz je włączone: niezbadane przejścia i niezbadana "
			"podłoga prowadzą do nowego terenu, a radar mówi ci, gdy zapisze nowe wyjście, które skaner potem wymienia "
			"wśród swoich wyjść. Szukaj tam: {den}. Gdy znajdzie się na liście, trasa i prowadzenie zaprowadzą cię do "
			"wejścia. Jeśli się zgubisz, kategoria niezbadany pokój skanera pokaże ci, dokąd iść."},
		{"Jesteś w: {den}. W jaskiniach i lochach mod zna pokoje: {view} i {y} podają wyjścia z pokoju, w którym "
			"jesteś. Wypróbuj to teraz.",
			"Jesteś w: {den}. W jaskiniach i lochach mod zna pokoje: Ctrl plus Y podaje wyjścia z pokoju, w którym "
			"jesteś. Wypróbuj to teraz."},
		{"Gdy wejdziesz do każdego pokoju, usłyszysz, czy jego części są jeszcze niezbadane. Zabij każdego potwora w: "
			"{den}; gra mówi, ilu zostało. Kategoria niezbadany pokój skanera pomaga ci nie pominąć żadnego.",
			"Gdy wejdziesz do każdego pokoju, usłyszysz, czy jego części są jeszcze niezbadane. Zabij każdego potwora w: "
			"{den}; gra mówi, ilu zostało. Kategoria niezbadany pokój skanera pomaga ci nie pominąć żadnego."},
		{"Brak już potworów w: {den}. Wróć do obozu: otwórz ekwipunek, użyj zwoju miejskiego portalu i wejdź do "
			"portalu przez {a}. Możesz też wrócić pieszo z trasą i prowadzeniem.",
			"Brak już potworów w: {den}. Wróć do obozu: otwórz ekwipunek przez I, przejdź do zwoju miejskiego portalu i "
			"naciśnij D, aby go przeczytać; zamknij ekwipunek i naciśnij D obok portalu. Możesz też wrócić pieszo z "
			"trasą i prowadzeniem."},
		{"Porozmawiaj z Akarą, aby odebrać nagrodę, punkt umiejętności. Potem otwórz drzewo umiejętności przez {menu} i wydaj go.",
			"Porozmawiaj z Akarą, aby odebrać nagrodę, punkt umiejętności. Potem otwórz drzewo umiejętności przez T i wydaj go."},
		{"Jeszcze jedno, czego gra ci nie mówi: punkty podróży strzegą twojej drogi. Gra zachowuje wykonane zadania, "
			"ale za każdym razem, gdy wracasz, zaczynasz w obozie, a do odległego obszaru dociera się przez jego punkt "
			"podróży, który jest twój dopiero, gdy go dotkniesz. Pierwszy poza obozem jest w: {plains}, za: {moor}. "
			"Znajdź go w kategorii punktów podróży skanera, podejdź do niego i naciśnij {a}: otworzy się jego menu i "
			"będzie twój. Zamknij menu przez {b}.",
			"Jeszcze jedno, czego gra ci nie mówi: punkty podróży strzegą twojej drogi. Gra zachowuje wykonane zadania, "
			"ale za każdym razem, gdy wracasz, zaczynasz w obozie, a do odległego obszaru dociera się przez jego punkt "
			"podróży, który jest twój dopiero, gdy go dotkniesz. Pierwszy poza obozem jest w: {plains}, za: {moor}. "
			"Znajdź go w kategorii punktów podróży skanera, podejdź do niego i naciśnij D: otworzy się jego menu i "
			"będzie twój. Zamknij menu przez Esc."},
		{"Samouczek ukończony. Wiesz, jak odnajdywać drogę: skaner, trasa, prowadzenie i radar. W większości menu "
			"{rs} w dół mówi, do czego służą tam przyciski: wypróbuj to w każdym menu, które jest dla ciebie nowe, na "
			"przykład w skrzyni osobistej. Na liście ustawień, przez {view} i {rb}, wybierasz, co mod mówi i odtwarza. "
			"Od teraz gra należy do ciebie.",
			"Samouczek ukończony. Wiesz, jak odnajdywać drogę: skaner, trasa, prowadzenie i radar. Na liście ustawień, "
			"przez Ctrl plus S, wybierasz, co mod mówi i odtwarza. Klawiatura ma też skróty, które idą za ciebie: H "
			"idzie do najbliższego niezbadanego pokoju, a Shift plus Home do tego, co wskazał skaner. Od teraz gra "
			"należy do ciebie."}},
	.radarTask = {
		"Teraz idź przez {ls}, aż usłyszysz ścianę.",
		"Teraz idź strzałkami, aż usłyszysz ścianę."},
	.cues = {"Przejście: otwór prowadzący do innego pokoju lub do wyjścia",
		"Niezbadane przejście: przejście z dwoma piskami na końcu; prowadzi do terenu, którego jeszcze nie widziałeś",
		"Wolna podłoga: możesz iść w tę stronę; dalej może być cokolwiek, także przejście",
		"Niezbadana podłoga: podłoga z dwoma piskami; w tę stronę leży teren, którego jeszcze nie widziałeś", "Ściana: blokuje ci drogę",
		"Drzwi: otwierają się, by cię przepuścić", "Mała przeszkoda: coś małego na twojej drodze, co możesz obejść"},
	.worldCues = {"Postać, ktoś do rozmowy",
		"Postać z wykrzyknikiem: ma coś dla ciebie",
		"Skrzynia osobista, skrzynia, w której trzymasz swoje przedmioty",
		"Schody, wejście lub portal",
		"Krawędź obszaru, droga do innego obszaru",
		"Niezbadany pokój",
		"Złoto na ziemi",
		"Mikstura na ziemi",
		"Przedmiot na ziemi, który możesz założyć, na przykład broń"},
	.worldTask = {
		"Teraz zabrzmią te wokół ciebie. Jeśli nie wiesz, czym jest dany dźwięk, skaner powie ci, co tam jest, "
		"a każdy dźwięk można wyłączyć na liście ustawień.",
		"Teraz zabrzmią te wokół ciebie. Jeśli nie wiesz, czym jest dany dźwięk, skaner powie ci, co tam jest, "
		"a każdy dźwięk można wyłączyć na liście ustawień."},
	.enemyCues = {"Potwór: najbliższe warczą z miejsca, w którym są",
		"Potwór atakujący z daleka, na przykład łucznik",
		"Szaman wskrzeszający martwe potwory: zabij go jako pierwszego",
		"Elita: czempion, unikatowy potwór lub boss",
		"W zasięgu: potwór wszedł w zasięg twojego ataku"},
	.leaveTask = {
		"Teraz opuść obóz sam, z tym, czego się nauczyłeś: wśród wyjść skanera znajdź: {moor}, poproś o trasę, "
		"podążaj za prowadzeniem i przekrocz wyjście.",
		"Teraz opuść obóz sam, z tym, czego się nauczyłeś: wśród wyjść skanera znajdź: {moor}, poproś o trasę, "
		"podążaj za prowadzeniem i przekrocz wyjście."},
	.auditionAgain = {
		"Następują jeden po drugim; nie musisz niczego naciskać. Aby usłyszeć dźwięk jeszcze raz, naciśnij {view} i "
		"{rs} w lewo: następny czeka wtedy do {view} i {rs} w prawo. Później cofanie się po tych komunikatach "
		"odtwarza każdy dźwięk ponownie.",
		"Następują jeden po drugim; nie musisz niczego naciskać. Aby usłyszeć dźwięk jeszcze raz, naciśnij K: "
		"następny czeka wtedy do L. Później cofanie się po tych komunikatach odtwarza każdy dźwięk ponownie."},
	.auditionWait = {
		"Po każdym dźwięku {view} i {rs} w lewo odtwarzają go jeszcze raz, a {view} i {rs} w prawo przechodzą do "
		"następnego. Później cofanie się po tych komunikatach odtwarza każdy dźwięk ponownie.",
		"Po każdym dźwięku K odtwarza go jeszcze raz, a L przechodzi do następnego. Później cofanie się po tych "
		"komunikatach odtwarza każdy dźwięk ponownie."},
	.portalButton = {
		"Szybciej: {portal} otwiera miejski portal od razu, bez ekwipunku, dopóki masz przy sobie zwój miejskiego "
		"portalu lub księgę miejskiego portalu.",
		"Szybciej: {portal} otwiera miejski portal od razu, bez ekwipunku, dopóki masz przy sobie zwój miejskiego "
		"portalu lub księgę miejskiego portalu."},
	.gold = {
		"Podniosłeś złoto; podnosi się je, gdy się po nim przejdzie. Gdy umrzesz, tracisz część złota, które nosisz "
		"przy sobie, ale złoto w skrzyni jest bezpieczne. Po powrocie do obozu otwórz skrzynię osobistą i wpłać to, "
		"czego nie potrzebujesz.",
		"Podniosłeś złoto; podnosi się je, gdy się po nim przejdzie. Gdy umrzesz, tracisz część złota, które nosisz "
		"przy sobie, ale złoto w skrzyni jest bezpieczne. Po powrocie do obozu otwórz skrzynię osobistą i wpłać to, "
		"czego nie potrzebujesz."},
	.shop = {
		"Sklep. Gdy tylko będziesz mieć złoto, kup: {portaltome} i {idtome}. Każda mieści do 20 zwojów w jednym "
		"polu, a zwoje jej rodzaju, które kupisz potem, trafiają do niej. Luźne zwoje zajmują po jednym polu i "
		"szybko zapełniają ekwipunek. Zwój identyfikacji pokazuje, czym jest niezidentyfikowany przedmiot: użyj go, "
		"a potem wybierz przedmiot.",
		"Sklep. Gdy tylko będziesz mieć złoto, kup: {portaltome} i {idtome}. Każda mieści do 20 zwojów w jednym "
		"polu, a zwoje jej rodzaju, które kupisz potem, trafiają do niej. Luźne zwoje zajmują po jednym polu i "
		"szybko zapełniają ekwipunek. Zwój identyfikacji pokazuje, czym jest niezidentyfikowany przedmiot: użyj go, "
		"a potem wybierz przedmiot."},
	.levelUp = {
		"Awansowałeś na wyższy poziom: masz punkty atrybutów i punkt umiejętności do wydania. Naciśnij {menu}, aby otworzyć "
		"ekran postaci; poprowadzę cię w środku.",
		"Awansowałeś na wyższy poziom: masz punkty atrybutów i punkt umiejętności do wydania. Naciśnij C, aby otworzyć "
		"ekran postaci; poprowadzę cię w środku."},
	.levelUpSheet = {
		"Twój ekran postaci. Jesteś na plusie Siły: {dpad} w górę i w dół przechodzą przez cztery atrybuty, a "
		"przytrzymanie {a} wydaje punkt na ten, na którym jesteś. {rs} w górę i w dół czytają twoje statystyki wiersz po wierszu, a "
		"w prawo mówi, ile punktów ci zostało.",
		"Twój ekran postaci. Strzałki przenoszą kursor na plus każdego atrybutu, a A wydaje na niego punkt. "
		"J czyta opis wiersza, na którym jesteś, a Shift ze strzałką w górę lub w dół przechodzi przez niego wiersz po "
		"wierszu."},
	.levelUpSwitch = {
		"Twoje punkty atrybutów są wydane. Teraz twój punkt umiejętności: naciskaj {rb}, aż dojdziesz do drzewa umiejętności; każde menu "
		"mówi swoją nazwę.",
		"Twoje punkty atrybutów są wydane. Teraz twój punkt umiejętności: naciśnij T, aby otworzyć drzewo umiejętności obok ekranu postaci."},
	.levelUpTree = {
		"Drzewo umiejętności. LT i RT zmieniają gałąź, {dpad} przechodzi przez jej umiejętności, a {rs} w górę czyta tę, na której "
		"jesteś. {a} wydaje na nią punkt, a {rs} w prawo mówi, ile punktów ci zostało. Gdy skończysz, zamknij "
		"je przez {b}.",
		"Drzewo umiejętności. Page Up i Page Down zmieniają gałąź, strzałki przechodzą przez jej umiejętności, a J czyta "
		"tę, na której jest kursor. A wydaje na nią punkt, a Ctrl plus Page Down mówi, ile punktów ci "
		"zostało. Gdy skończysz, zamknij je przez Esc."},
	.levelUpTreeLater = {
		"Nadal masz swój punkt umiejętności. Naciśnij {menu}: gdy atrybuty są wydane, otwiera drzewo umiejętności.",
		"Nadal masz swój punkt umiejętności. Naciśnij T, aby otworzyć drzewo umiejętności."},
	.levelUpBind = {
		"Aby użyć umiejętności, przypisz ją do przycisku: w drzewie umiejętności, na umiejętności, naciśnij {x}, a "
		"potem przycisk, do którego ma trafić. Odczyt każdej umiejętności mówi, na którym przycisku jest. "
		"Umiejętności pasywne, takie jak biegłości, działają same: nie potrzebują przycisku i nie mogą go mieć. "
		"Przyciskami LT i RT, za gałęziami twojej klasy, wchodzisz na stronę {general}; zawiera to, co ma każda "
		"postać, na przykład Miejski portal i Identyfikację; przypisujesz je do przycisków tak samo.",
		"Aby użyć umiejętności, włóż ją do ręki: S otwiera listę umiejętności dla prawej ręki, a Shift plus S tę dla "
		"lewej; strzałki wybierają jedną, a A wkłada ją do tej ręki. Przy otwartej liście od F1 do F8 przypisują "
		"umiejętność pod kursorem do tego klawisza. Umiejętności pasywne, takie jak biegłości, działają same: nie ma "
		"ich na tych listach i nie trzeba nic więcej."},
	.kit = {
		"Podniosłeś coś do noszenia. Naciśnij {menu}, aby otworzyć menu; otwierają się na twoim ekwipunku, a "
		"ja poprowadzę cię w środku.",
		"Podniosłeś coś do noszenia. Naciśnij I, aby otworzyć ekwipunek; poprowadzę cię w środku."},
	.kitOpen = {
		"Twój ekwipunek. {dpad} przechodzi z pola na pole, przez plecak i to, co masz na sobie, a mod mówi, "
		"co zawiera każde. Na przedmiocie {rs} w górę czyta jego opis, a {rs} w dół przyciski, których możesz z nim użyć, "
		"w tym ten, który go zakłada. Przedmiot, który mówi nie zidentyfikowany, nie może być noszony, dopóki nie zostanie zidentyfikowany, "
		"zwojem identyfikacji, który sprzedaje Akara.",
		"Twój ekwipunek. Strzałki przenoszą kursor z pola na pole, przez plecak i to, co masz "
		"na sobie, a mod mówi, co zawiera każde. J czyta opis przedmiotu, a Shift plus A go zakłada. Przedmiot, "
		"który mówi nie zidentyfikowany, nie może być noszony, dopóki nie zostanie zidentyfikowany, zwojem identyfikacji, który sprzedaje Akara."},
	.kitWorn = {
		"Masz go na sobie. Zamknij ekwipunek przez {b}, gdy skończysz.",
		"Masz go na sobie. Zamknij ekwipunek przez Esc, gdy skończysz."},
	.kitDone = {
		"Gotowe. Cokolwiek podniesiesz, w ekwipunku to zakładasz. Wracamy do tego, co robiłeś:",
		"Gotowe. Cokolwiek podniesiesz, w ekwipunku to zakładasz. Wracamy do tego, co robiłeś:"},
	.levelUpDone = {
		"Gotowe. Wypróbuj teraz raz swoją nową umiejętność: naciśnij przycisk, do którego ją przypisałeś, i "
		"posłuchaj, jak brzmi. Za każdym razem, gdy awansujesz na wyższy poziom, {menu} otwiera ekran postaci. "
		"Wracamy do tego, co robiłeś:",
		"Gotowe. Wypróbuj teraz raz swoją nową umiejętność: A używa umiejętności lewej ręki, a W prawej; posłuchaj, "
		"jak brzmi. Za każdym razem, gdy awansujesz na wyższy poziom, C otwiera ekran postaci, a T drzewo "
		"umiejętności. Wracamy do tego, co robiłeś:"},
	.routeStick = "{view} i {rs} w dół",
	.routeButton = "{view} i {a}",
	.firstMessage = "Pierwszy komunikat",
	.lastMessage = "Ostatni komunikat",
	.den = "Siedlisko Zła",
	.moor = "Krwawe Wrzosowisko",
	.plains = "Zimna Równina",
	.portalTome = "Księga miejskiego portalu",
	.identifyTome = "Księga identyfikacji",
	.generalSkills = "Ogólne umiejętności",
};
}
