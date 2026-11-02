#pragma once
#include "tutorial_text.h"

// German (deDE): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts deDE{
	.welcome = {
		"D2R-Access-Tutorial. Ich zeige dir, wie du dich mit der Mod zurechtfindest, bis die erste Quest erledigt ist. "
		"Jeder Schritt bittet dich, etwas zu tun; wenn du es getan hast, ertönt der Erledigt-Klang und der nächste Schritt folgt. "
		"{view} und {rs} nach links oder rechts gehen durch diese Meldungen; {view} und {up} springen zur ersten zurück. "
		"Du kannst das Tutorial in der Einstellungsliste ausschalten, mit {view} und {rb}.",
		"D2R-Access-Tutorial. Ich zeige dir, wie du dich mit der Mod zurechtfindest, bis die erste Quest erledigt ist. "
		"Jeder Schritt bittet dich, etwas zu tun; wenn du es getan hast, ertönt der Erledigt-Klang und der nächste Schritt folgt. "
		"K und L gehen durch diese Meldungen; Umschalt plus K springt zur ersten zurück. "
		"Du kannst das Tutorial in der Einstellungsliste ausschalten, mit Strg plus S."},
	.steps = {
		{"Erster Schritt: dein Zustand. Drücke {view} und {x}, um dein Leben und dein Mana zu hören.",
			"Erster Schritt: dein Zustand. Drücke Z, um dein Leben zu hören, und X für dein Mana."},
		{"Das Radar klingt in vier Richtungen, während sich deine Umgebung ändert; nach unten klingt es tiefer. Ich "
			"spiele jetzt seine Klänge ab, jeden nach seinem Namen und seiner Bedeutung.",
			"Das Radar klingt in vier Richtungen, während sich deine Umgebung ändert; nach unten klingt es tiefer. Ich "
			"spiele jetzt seine Klänge ab, jeden nach seinem Namen und seiner Bedeutung."},
		{"Neben dem Radar klingen Dinge in deiner Nähe von dort, wo sie sind, und zwar lauter, je näher sie sind: "
			"Personen, Orte und was am Boden liegt, die Gegenstände mit den Klängen, die du in der Einstellungsliste "
			"gewählt hast. Bisher habe ich sie leise gehalten, damit du das Radar allein hören konntest. Ich spiele "
			"jeden nach seinem Namen ab.",
			"Neben dem Radar klingen Dinge in deiner Nähe von dort, wo sie sind, und zwar lauter, je näher sie sind: "
			"Personen, Orte und was am Boden liegt, die Gegenstände mit den Klängen, die du in der Einstellungsliste "
			"gewählt hast. Bisher habe ich sie leise gehalten, damit du das Radar allein hören konntest. Ich spiele "
			"jeden nach seinem Namen ab."},
		{"Der Scanner. Er listet auf, was auf der Karte ist, nach Kategorien: Ausgänge, NPCs, Wegpunkte und mehr, das Nächste "
			"zuerst, mit Entfernung und Richtung. Mit {rs} ändern oben und unten die Kategorie, und links und rechts gehen "
			"durch die Liste. Finde Warriv in der Kategorie NPCs: Er ist der, der dir am nächsten ist.",
			"Der Scanner. Er listet auf, was auf der Karte ist, nach Kategorien: Ausgänge, NPCs, Wegpunkte und mehr, das Nächste "
			"zuerst, mit Entfernung und Richtung. Strg plus Bild auf und Strg plus Bild ab ändern die Kategorie; Bild auf und "
			"Bild ab gehen durch die Liste. Finde Warriv in der Kategorie NPCs: Er ist der, der dir am nächsten ist."},
		{"Sprich mit Warriv. Er ist ein paar Schritte entfernt: Geh mit {ls} zu ihm, so wie der Scanner es gesagt hat, drücke {a} "
			"neben ihm und hör dir an, was er dir zu sagen hat. Wenn er fertig ist, schließe den Dialog mit {b}.",
			"Sprich mit Warriv. Er ist ein paar Schritte entfernt: Geh mit den Pfeiltasten zu ihm, so wie der Scanner es gesagt hat, "
			"drücke D neben ihm und hör dir an, was er dir zu sagen hat. Wenn er fertig ist, schließe den Dialog mit Esc."},
		{"Jetzt Akara, die Heilerin des Lagers: Sie hat die erste Quest. Sie ist weiter entfernt; finde sie in der Kategorie "
			"NPCs des Scanners.",
			"Jetzt Akara, die Heilerin des Lagers: Sie hat die erste Quest. Sie ist weiter entfernt; finde sie in der Kategorie "
			"NPCs des Scanners."},
		{"Der Weg. Er beschreibt dir den Weg zu dem, was der Scanner genannt hat, in Richtungen und Entfernungen, so wie du ihn "
			"auf der Minikarte planen würdest; er läuft nicht für dich. Drücke {route}, wenn Akara genannt ist.",
			"Der Weg. Er beschreibt dir den Weg zu dem, was der Scanner genannt hat, in Richtungen und Entfernungen, so wie du ihn "
			"auf der Minikarte planen würdest; er läuft nicht für dich. Drücke Strg plus E, wenn Akara genannt ist."},
		{"Die Zielführung. Sie sendet Signale in Richtung dessen, was der Scanner genannt hat, und führt dich den Weg entlang, "
			"aber du läufst selbst. Drücke {view} und {lb}, um Akara zu folgen, und lauf auf die Signale zu, bis du sie erreichst; "
			"ein weiterer Druck lässt los.",
			"Die Zielführung. Sie sendet Signale in Richtung dessen, was der Scanner genannt hat, und führt dich den Weg entlang, "
			"aber du läufst selbst. Drücke Strg plus B, um Akara zu folgen, und lauf mit den Pfeiltasten auf die Signale zu, "
			"bis du sie erreichst; ein weiterer Druck lässt los."},
		{"Sprich mit Akara: Drücke neben ihr {a}. Sie hat eine Quest für dich. "
			"Wenn sie fertig ist, schließe den Dialog mit {b}.",
			"Sprich mit Akara: Drücke neben ihr D. Sie hat eine Quest für dich. "
			"Wenn sie fertig ist, schließe den Dialog mit Esc."},
		{"Außerhalb des Lagers gibt es Monster, und jede Art klingt auf ihre eigene Weise von dort, wo sie ist. Bevor "
			"du gehst, spiele ich sie ab, jedes nach seinem Namen.",
			"Außerhalb des Lagers gibt es Monster, und jede Art klingt auf ihre eigene Weise von dort, wo sie ist. Bevor "
			"du gehst, spiele ich sie ab, jedes nach seinem Namen."},
		{"Du bist in {moor}. Im Freien verorten dich zwei Fragen: {view} und {b} geben einen Überblick über das "
			"Gelände, und {view} und {rs} nach oben die Beschreibung des Gebiets. Probiere den Überblick aus.",
			"Du bist in {moor}. Strg plus O gibt dir einen Überblick über das Gelände um dich herum. Probiere es aus."},
		{"Hier gibt es Monster: Sie klingen so, wie du es im Lager gehört hast, und der Scanner listet sie in einer "
			"eigenen Kategorie auf. Greife mit {a} an; deine anderen Fertigkeiten liegen auf den Tasten, die du ihnen "
			"gegeben hast. Der Doppelton des Zielsignals sagt dir, dass dein Angriff auf ein Monster gerichtet ist: "
			"Greife an, wenn du ihn hörst, und wenn es still bleibt, dreh dich zum Knurren, bis es piept, damit kein "
			"Angriff an eine Wand oder ein Fass verschwendet wird. Wenn dein Leben sinkt, trinke einen Trank mit {dpad}. Was sie fallen lassen, klingt von dort, "
			"wo es liegt: Geh hin und drücke {a}, um es aufzuheben; Gold wird beim Darüberlaufen aufgehoben. Töte ein "
			"Monster.",
			"Hier gibt es Monster: Sie klingen so, wie du es im Lager gehört hast, und der Scanner listet sie in einer "
			"eigenen Kategorie auf. Tab wählt das nächste Monster; halte A, um mit deiner linken Fertigkeit anzugreifen, "
			"und W für die rechte. Der Doppelton des Zielsignals sagt dir, dass dein Angriff auf ein Monster gerichtet "
			"ist: Greife an, wenn du ihn hörst, damit kein Angriff an eine Wand oder ein Fass verschwendet wird. Wenn "
			"dein Leben sinkt, liegen die Tränke auf den Tasten 1 bis 4. Was sie fallen "
			"lassen, klingt von dort, wo es liegt: Punkt wählt den nächsten Gegenstand am Boden, Komma den vorherigen, "
			"und D hebt ihn auf; Gold wird beim Darüberlaufen aufgehoben. Töte ein Monster."},
		{"Finde {den}. Die Karte wird für jedes Spiel neu erzeugt, also kann ihr Eingang nah oder fern sein. Folge "
			"den Klängen des Radars oder den Wandtönen, wenn du sie an hast: Unerkundete Durchgänge und unerkundeter "
			"Boden führen zu neuem Gebiet, und das Radar sagt dir, wenn es einen neuen Ausgang verzeichnet, den der "
			"Scanner dann unter seinen Ausgängen listet. Suche dort nach {den}: Sobald sie gelistet ist, bringen dich "
			"der Weg und die Zielführung zu ihrem Eingang. Wenn du dich verirrst, zeigt dir die Kategorie unerkundetes "
			"Gebiet des Scanners, wohin du gehen sollst.",
			"Finde {den}. Die Karte wird für jedes Spiel neu erzeugt, also kann ihr Eingang nah oder fern sein. Folge "
			"den Klängen des Radars oder den Wandtönen, wenn du sie an hast: Unerkundete Durchgänge und unerkundeter "
			"Boden führen zu neuem Gebiet, und das Radar sagt dir, wenn es einen neuen Ausgang verzeichnet, den der "
			"Scanner dann unter seinen Ausgängen listet. Suche dort nach {den}: Sobald sie gelistet ist, bringen dich "
			"der Weg und die Zielführung zu ihrem Eingang. Wenn du dich verirrst, zeigt dir die Kategorie unerkundetes "
			"Gebiet des Scanners, wohin du gehen sollst."},
		{"Du bist in {den}. In Höhlen und Verliesen kennt die Mod die Räume: {view} und {y} nennen die Wege aus dem "
			"Raum, in dem du bist. Probiere es jetzt aus.",
			"Du bist in {den}. In Höhlen und Verliesen kennt die Mod die Räume: Strg plus Y nennt die Wege aus dem Raum, "
			"in dem du bist. Probiere es jetzt aus."},
		{"Wenn du einen Raum betrittst, hörst du, ob Teile davon noch unerkundet sind. Töte jedes Monster in {den}; "
			"das Spiel sagt, wie viele noch übrig sind. Die Kategorie unerkundetes Gebiet des Scanners hilft dir, keines "
			"zu übersehen.",
			"Wenn du einen Raum betrittst, hörst du, ob Teile davon noch unerkundet sind. Töte jedes Monster in {den}; "
			"das Spiel sagt, wie viele noch übrig sind. Die Kategorie unerkundetes Gebiet des Scanners hilft dir, keines "
			"zu übersehen."},
		{"{den} ist leer. Geh zurück ins Lager: Öffne das Inventar, benutze die Schriftrolle des Stadtportals und "
			"tritt mit {a} in das Portal. Du kannst auch mit dem Weg und der Zielführung zurücklaufen.",
			"{den} ist leer. Geh zurück ins Lager: Öffne das Inventar mit I, geh zur Schriftrolle des Stadtportals und "
			"drücke D, um sie zu lesen; schließe das Inventar und drücke D neben dem Portal. Du kannst auch mit dem Weg "
			"und der Zielführung zurücklaufen."},
		{
			"Sprich mit Akara für deine Belohnung, einen Fertigkeitspunkt: Drücke neben ihr {a}.",
			"Sprich mit Akara für deine Belohnung, einen Fertigkeitspunkt: Drücke neben ihr D."},
		{
			"Akara verkauft auch Bücher, die Platz im Inventar sparen. Wähle in ihrem Menü {trade} mit {dpad} und "
			"drücke {a}; wenn du es geschlossen hast, sprich noch einmal mit ihr. Drinnen sage ich dir, wie du "
			"kaufst.",
			"Akara verkauft auch Bücher, die Platz im Inventar sparen. Wähle in ihrem Menü {trade} mit den "
			"Pfeiltasten und drücke A; wenn du es geschlossen hast, sprich mit D noch einmal mit ihr. Drinnen "
			"sage ich dir, wie du kaufst."},
		{
			"Gib jetzt deinen Fertigkeitspunkt aus: Öffne den Fertigkeitsbaum mit {menu}. LT und RT wechseln den "
			"Zweig, {dpad} geht durch seine Fertigkeiten, und {a} gedrückt halten gibt den Punkt für die aus, auf "
			"der du bist. Wenn du fertig bist, schließe ihn mit {b}.",
			"Gib jetzt deinen Fertigkeitspunkt aus: Öffne den Fertigkeitsbaum mit T. Bild auf und Bild ab "
			"wechseln den Zweig, die Pfeiltasten gehen durch seine Fertigkeiten, und A gibt den Punkt für die "
			"aus, auf der der Cursor ist. Wenn du fertig bist, schließe ihn mit Esc."},
		{
			"Noch etwas, das dir das Spiel nicht sagt: Wegpunkte bewahren deinen Weg. Jedes Mal, wenn du "
			"zurückkommst, beginnst du im Lager, und ein fernes Gebiet erreichst du über seinen Wegpunkt, der "
			"erst dir gehört, wenn du ihn berührt hast. Das Lager hat seinen eigenen: Finde ihn in der Kategorie "
			"Wegpunkte des Scanners, lass dir den Weg beschreiben, folge der Zielführung und drücke {a} neben "
			"ihm, um sein Menü zu öffnen.",
			"Noch etwas, das dir das Spiel nicht sagt: Wegpunkte bewahren deinen Weg. Jedes Mal, wenn du "
			"zurückkommst, beginnst du im Lager, und ein fernes Gebiet erreichst du über seinen Wegpunkt, der "
			"erst dir gehört, wenn du ihn berührt hast. Das Lager hat seinen eigenen: Finde ihn in der Kategorie "
			"Wegpunkte des Scanners, lass dir den Weg beschreiben, folge der Zielführung und drücke D neben ihm, "
			"um sein Menü zu öffnen."},
		{"Tutorial abgeschlossen. Du weißt, wie du dich zurechtfindest: Scanner, Weg, Zielführung und Radar. In den "
			"meisten Menüs sagt dir {rs} nach unten, was die Tasten dort tun: Probiere es in jedem Menü, das dir neu "
			"ist, etwa in der Persönlichen Truhe. In der Einstellungsliste, mit {view} und {rb}, wählst du, was die Mod "
			"sagt und abspielt. Ab jetzt gehört das Spiel dir.",
			"Tutorial abgeschlossen. Du weißt, wie du dich zurechtfindest: Scanner, Weg, Zielführung und Radar. In der "
			"Einstellungsliste, mit Strg plus S, wählst du, was die Mod sagt und abspielt. Die Tastatur hat auch "
			"Kurzbefehle, die für dich laufen: H geht zum nächsten unerkundeten Gebiet, und Umschalt plus Pos1 zu dem, was "
			"der Scanner genannt hat. Ab jetzt gehört das Spiel dir."}},
	.radarTask = {
		"Geh jetzt mit {ls}, bis du eine Wand hörst.",
		"Geh jetzt mit den Pfeiltasten, bis du eine Wand hörst."},
	.cues = {"Durchgang: eine Öffnung, die zu einem anderen Raum oder zu einem Ausgang führt",
		"Unerkundeter Durchgang: der Durchgang mit zwei Pieptönen am Ende; er führt zu Gebiet, das du noch nicht gesehen hast",
		"Freier Boden: Du kannst in diese Richtung gehen; weiter vorn kann alles sein, auch ein Durchgang",
		"Unerkundeter freier Boden: der Boden mit den zwei Pieptönen; in dieser Richtung liegt Gebiet, das du noch nicht gesehen hast", "Wand: Sie versperrt dir den Weg",
		"Tür: Sie öffnet sich, um dich durchzulassen", "Kleines Hindernis: etwas Kleines in deinem Weg, um das du herumgehen kannst"},
	.worldCues = {"NPC, jemand zum Reden",
		"NPC mit Ausrufezeichen: Er hat etwas für dich",
		"Persönliche Truhe, die Truhe, in der du deine Gegenstände aufbewahrst",
		"Treppe, Eingang oder Portal",
		"Gebietsgrenze, der Weg in ein anderes Gebiet",
		"Unerkundetes Gebiet",
		"Gold am Boden",
		"Ein Trank am Boden",
		"Ein Gegenstand am Boden, den du anlegen kannst, etwa eine Waffe"},
	.worldTask = {
		"Jetzt klingen die Dinge um dich herum. Wenn du nicht weißt, was ein Klang ist, sagt dir der Scanner, was dort ist, "
		"und jeder Klang lässt sich in der Einstellungsliste ausschalten.",
		"Jetzt klingen die Dinge um dich herum. Wenn du nicht weißt, was ein Klang ist, sagt dir der Scanner, was dort ist, "
		"und jeder Klang lässt sich in der Einstellungsliste ausschalten."},
	.enemyCues = {"Monster: Die nächsten knurren von dort, wo sie sind",
		"Monster, das aus der Ferne angreift, etwa ein Bogenschütze",
		"Schamane, der tote Monster wieder zum Leben erweckt: Töte ihn zuerst",
		"Elite: ein Champion, ein einzigartiges Monster oder ein Boss",
		"In Reichweite: Ein Monster ist in die Reichweite deines Angriffs gekommen",
		"Zielsignal: Dein Angriff ist auf ein Monster gerichtet, das du siehst"},
	.leaveTask = {
		"Verlasse jetzt das Lager allein, mit dem, was du gelernt hast: Finde unter den Ausgängen des Scanners "
		"{moor}, frag nach dem Weg, folge der Zielführung und überquere den Ausgang.",
		"Verlasse jetzt das Lager allein, mit dem, was du gelernt hast: Finde unter den Ausgängen des Scanners "
		"{moor}, frag nach dem Weg, folge der Zielführung und überquere den Ausgang."},
	.auditionAgain = {
		"Sie kommen nacheinander; du musst nichts drücken. Um einen Klang noch einmal zu hören, drücke {view} und "
		"{rs} nach links: Der nächste wartet dann, bis {view} und {rs} nach rechts. Später spielt das Zurückgehen "
		"durch diese Meldungen jeden Klang noch einmal ab.",
		"Sie kommen nacheinander; du musst nichts drücken. Um einen Klang noch einmal zu hören, drücke K: Der "
		"nächste wartet dann bis L. Später spielt das Zurückgehen durch diese Meldungen jeden Klang noch einmal ab."},
	.auditionWait = {
		"Nach jedem Klang spielen {view} und {rs} nach links ihn noch einmal ab, und {view} und {rs} nach rechts "
		"geht zum nächsten weiter. Später spielt das Zurückgehen durch diese Meldungen jeden Klang noch einmal ab.",
		"Nach jedem Klang spielt K ihn noch einmal ab, und L geht zum nächsten weiter. Später spielt das Zurückgehen "
		"durch diese Meldungen jeden Klang noch einmal ab."},
	.portalButton = {
		"Schneller: {portal} öffnet sofort ein Stadtportal, ohne das Inventar, solange du eine Schriftrolle oder "
		"einen Foliant des Stadtportals bei dir hast.",
		"Schneller: {portal} öffnet sofort ein Stadtportal, ohne das Inventar, solange du eine Schriftrolle oder "
		"einen Foliant des Stadtportals bei dir hast."},
	.gold = {
		"Du hast Gold aufgehoben; es wird beim Darüberlaufen aufgehoben. Wenn du stirbst, verlierst du einen Teil "
		"des Goldes, das du bei dir trägst, aber Gold in der Persönlichen Truhe ist sicher. Zurück im Lager öffne "
		"die Persönliche Truhe und zahle ein, was du nicht brauchst.",
		"Du hast Gold aufgehoben; es wird beim Darüberlaufen aufgehoben. Wenn du stirbst, verlierst du einen Teil "
		"des Goldes, das du bei dir trägst, aber Gold in der Persönlichen Truhe ist sicher. Zurück im Lager öffne "
		"die Persönliche Truhe und zahle ein, was du nicht brauchst."},
	.shop = {
		"Ein Laden. Was er verkauft, steht auf einer Seite, in Reitern, die du mit LT und RT wechselst, und "
		"dein Inventar auf der anderen. {dpad} geht von Feld zu Feld, und {rs} nach oben liest die "
		"Beschreibung des Gegenstands. Suche einen {portaltome} und einen {idtome}, die Akara verkauft; "
		"drücke auf einem davon {a} und bestätige, um ihn zu kaufen, wenn du das Gold hast. Jeder fasst bis "
		"zu 20 Schriftrollen auf einem Platz von einer Spalte Breite und zwei Zeilen Höhe, und die "
		"Schriftrollen seiner Art, die du danach kaufst, kommen hinein; einzelne Schriftrollen belegen je ein "
		"Feld und füllen das Inventar schnell. Eine Schriftrolle der Identifikation zeigt, was ein nicht "
		"identifizierter Gegenstand ist: Benutze sie und wähle dann den Gegenstand. Schließe den Laden mit "
		"{b}."
		" Auch wenn du nichts kaufst, geht das Tutorial weiter; die Bücher können warten.",
		"Ein Laden. Was er verkauft, steht auf einer Seite, in Reitern, die du mit Bild auf und Bild ab "
		"wechselst, und dein Inventar auf der anderen. Die Pfeiltasten bringen den Cursor von Feld zu Feld, "
		"und J liest die Beschreibung des Gegenstands. Suche einen {portaltome} und einen {idtome}, die Akara "
		"verkauft; drücke auf einem davon D, um ihn zu kaufen, wenn du das Gold hast. Jeder fasst bis zu 20 "
		"Schriftrollen auf einem Platz von einer Spalte Breite und zwei Zeilen Höhe, und die Schriftrollen "
		"seiner Art, die du danach kaufst, kommen hinein; einzelne Schriftrollen belegen je ein Feld und "
		"füllen das Inventar schnell. Eine Schriftrolle der Identifikation zeigt, was ein nicht "
		"identifizierter Gegenstand ist: Benutze sie und wähle dann den Gegenstand. Schließe den Laden mit "
		"Esc."
		" Auch wenn du nichts kaufst, geht das Tutorial weiter; die Bücher können warten."},
	.levelUp = {
		"Du bist eine Stufe aufgestiegen: Du hast Wertpunkte und einen Fertigkeitspunkt auszugeben. Drücke "
		"{menu}: Dein Charakterbildschirm oder dein Fertigkeitsbaum öffnet sich; ich führe dich darin.",
		"Du bist eine Stufe aufgestiegen: Du hast Wertpunkte und einen Fertigkeitspunkt auszugeben. Drücke C, "
		"um deinen Charakterbildschirm zu öffnen, oder T für den Fertigkeitsbaum; ich führe dich darin."},
	.levelUpSheet = {
		"Dein Charakterbildschirm. Du bist auf dem Plus von Stärke: {dpad} nach oben und unten gehen durch die vier Attribute, und "
		"{a} gedrückt halten gibt einen Punkt für das aus, auf dem du bist; {x} gedrückt halten gibt fünf auf einmal aus. {rs} nach oben und unten lesen deine Werte Zeile für Zeile, und "
		"nach rechts nennt die Punkte, die dir bleiben.",
		"Dein Charakterbildschirm. Die Pfeiltasten bringen den Cursor zum Plus jedes Attributs, und A gibt einen Punkt dafür aus. "
		"J liest die Beschreibung der Zeile, auf der du bist, und Umschalt mit der Pfeiltaste nach oben oder unten geht Zeile für "
		"Zeile durch."},
	.levelUpSwitch = {
		"Deine Wertpunkte sind ausgegeben. Jetzt dein Fertigkeitspunkt: Drücke {rb}, bis du beim Fertigkeitsbaum bist; jedes Menü "
		"nennt seinen Namen.",
		"Deine Wertpunkte sind ausgegeben. Jetzt dein Fertigkeitspunkt: Drücke T, um den Fertigkeitsbaum neben deinem Charakterbildschirm zu öffnen."},
	.levelUpTree = {
		"Der Fertigkeitsbaum. LT und RT wechseln den Zweig, {dpad} geht durch seine Fertigkeiten, und {rs} "
		"nach oben liest die, auf der du bist. {a} gedrückt halten gibt den Punkt dafür aus, und {rs} nach "
		"rechts nennt die Punkte, die dir bleiben. Wenn du fertig bist, schließe ihn mit {b}.",
		"Der Fertigkeitsbaum. Bild auf und Bild ab wechseln den Zweig, die Pfeiltasten gehen durch seine Fertigkeiten, und J liest "
		"die, auf der der Cursor ist. A gibt den Punkt dafür aus, und Strg plus Bild ab nennt die Punkte, die dir "
		"bleiben. Wenn du fertig bist, schließe ihn mit Esc."},
	.levelUpTreeLater = {
		"Du hast noch deinen Fertigkeitspunkt. Drücke {menu}: Wenn die Wertpunkte ausgegeben sind, öffnet es den Fertigkeitsbaum.",
		"Du hast noch deinen Fertigkeitspunkt. Drücke T, um den Fertigkeitsbaum zu öffnen."},
	.levelUpToSheet = {
		"Dein Fertigkeitspunkt ist ausgegeben. Es bleiben Wertpunkte: Drücke {lb}, bis du bei deinem "
		"Charakterbildschirm bist; jedes Menü nennt seinen Namen.",
		"Dein Fertigkeitspunkt ist ausgegeben. Es bleiben Wertpunkte: Drücke C, um deinen Charakterbildschirm "
		"zu öffnen."},
	.levelUpSheetLater = {
		"Du hast noch Wertpunkte. Drücke {menu} und gehe mit {lb} oder {rb} zu deinem Charakterbildschirm; "
		"jedes Menü nennt seinen Namen.",
		"Du hast noch Wertpunkte. Drücke C, um deinen Charakterbildschirm zu öffnen."},
	.levelUpBind = {
		"Um eine Fertigkeit zu benutzen, lege sie auf eine Taste: Drücke im Fertigkeitsbaum auf der Fertigkeit {x} "
		"und dann die Taste, auf die du sie legen willst. Die Ansage jeder Fertigkeit nennt, auf welcher Taste sie "
		"liegt. Passive Fertigkeiten, etwa die Meisterschaften, wirken von selbst: Sie brauchen keine Taste und "
		"können auch keine haben. Mit LT und RT findest du hinter den Zweigen deiner Klasse die Seite {general}; sie "
		"enthält, was jeder Charakter hat, etwa Stadtportal und Identifizieren; du legst sie auf dieselbe Weise auf "
		"Tasten.",
		"Um eine Fertigkeit zu benutzen, lege sie in eine Hand: S öffnet die Liste der Fertigkeiten für die rechte "
		"Hand und Umschalt plus S die für die linke; die Pfeiltasten wählen eine aus, und A legt sie in diese Hand. "
		"Bei geöffneter Liste binden F1 bis F8 die Fertigkeit unter dem Cursor an diese Taste. Passive Fertigkeiten, "
		"etwa die Meisterschaften, wirken von selbst: Sie stehen nicht in diesen Listen und brauchen nichts weiter."},
	.kit = {
		"Du hast etwas zum Anziehen aufgehoben. Drücke {menu}, um die Menüs zu öffnen; sie öffnen sich auf deinem Inventar, und ich "
		"führe dich darin.",
		"Du hast etwas zum Anziehen aufgehoben. Drücke I, um dein Inventar zu öffnen; ich führe dich darin."},
	.kitOpen = {
		"Dein Inventar. {dpad} geht von Feld zu Feld, durch deinen Rucksack und das, was du anhast, und die Mod sagt, "
		"was jedes enthält. Auf einem Gegenstand liest {rs} nach oben seine Beschreibung und {rs} nach unten die Tasten, die du damit benutzen kannst, "
		"darunter die, die ihn anlegt. Ein Gegenstand, der \"nicht identifiziert\" sagt, kann erst getragen werden, wenn er identifiziert ist, "
		"mit einer Schriftrolle der Identifikation, die Akara verkauft.",
		"Dein Inventar. Die Pfeiltasten bringen den Cursor von Feld zu Feld, durch deinen Rucksack und das, was du "
		"anhast, und die Mod sagt, was jedes enthält. J liest die Beschreibung eines Gegenstands, und Umschalt plus A legt ihn an. Ein Gegenstand, "
		"der \"nicht identifiziert\" sagt, kann erst getragen werden, wenn er identifiziert ist, mit einer Schriftrolle der Identifikation, die Akara verkauft."},
	.kitWorn = {
		"Du trägst ihn. Schließe das Inventar mit {b}, wenn du fertig bist.",
		"Du trägst ihn. Schließe das Inventar mit Esc, wenn du fertig bist."},
	.kitDone = {
		"Fertig. Was du auch aufhebst, im Inventar ziehst du es an. Zurück zu dem, was du getan hast:",
		"Fertig. Was du auch aufhebst, im Inventar ziehst du es an. Zurück zu dem, was du getan hast:"},
	.levelUpDone = {
		"Fertig. Probiere jetzt einmal deine neue Fertigkeit aus: Drücke die Taste, auf die du sie gelegt hast, und "
		"höre, wie sie klingt. Immer wenn du eine Stufe aufsteigst, öffnet {menu} deinen Charakterbildschirm. Zurück "
		"zu dem, was du getan hast:",
		"Fertig. Probiere jetzt einmal deine neue Fertigkeit aus: A benutzt die Fertigkeit deiner linken Hand und W "
		"die der rechten; höre, wie sie klingt. Immer wenn du eine Stufe aufsteigst, öffnet C deinen "
		"Charakterbildschirm und T den Fertigkeitsbaum. Zurück zu dem, was du getan hast:"},
	.waypointMenu = {
		"Das Wegpunktmenü. {dpad} nach oben und unten geht durch die Gebiete des Akts, die einen Wegpunkt "
		"haben: Bei denen, die unentdeckt sagen, hast du ihn noch nicht berührt, und das Gebiet, das du bist "
		"hier sagt, ist dieses. Auf einem entdeckten Gebiet bringt dich {a} sofort dorthin; LT und RT "
		"wechseln den Akt. Im Moment hast du nur diesen; der nächste ist in {plains}, hinter {moor}: Berühre "
		"ihn, und von da an reist du in einem Augenblick zwischen beiden. Schließe das Menü mit {b}.",
		"Das Wegpunktmenü. Die Pfeiltasten nach oben und unten gehen durch die Gebiete des Akts, die einen "
		"Wegpunkt haben: Bei denen, die unentdeckt sagen, hast du ihn noch nicht berührt, und das Gebiet, das "
		"du bist hier sagt, ist dieses. Auf einem entdeckten Gebiet bringt dich A sofort dorthin; Bild auf "
		"und Bild ab wechseln den Akt. Im Moment hast du nur diesen; der nächste ist in {plains}, hinter "
		"{moor}: Berühre ihn, und von da an reist du in einem Augenblick zwischen beiden. Schließe das Menü "
		"mit Esc."},
	.routeStick = "{view} und {rs} nach unten",
	.routeButton = "{view} und {a}",
	.firstMessage = "Erste Meldung",
	.lastMessage = "Letzte Meldung",
	.den = "Höhle des Bösen",
	.moor = "Blutmoor",
	.plains = "Kalte Ebene",
	.portalTome = "Foliant des Stadtportals",
	.identifyTome = "Foliant der Identifikation",
	.generalSkills = "Allgemeine Fertigkeiten",
	.trade = "Handeln",
	.offer = {
		"Möchtest du das Tutorial von D2R Access, das dir zeigt, wie du die Mod benutzt? Drücke zweimal {a} (Kreuz auf PlayStation), um es zu beginnen, zweimal {b} (Kreis) für jetzt nicht oder zweimal {y} (Dreieck), damit ich nie wieder frage. Bis du antwortest, nimmt das Spiel keine Eingabe an.",
		"Möchtest du das Tutorial von D2R Access, das dir zeigt, wie du die Mod benutzt? Drücke zweimal Eingabe, um es zu beginnen, zweimal Esc für jetzt nicht oder zweimal Rücktaste, damit ich nie wieder frage. Bis du antwortest, nimmt das Spiel keine Eingabe an."},
	.offerLater = {"Vorerst kein Tutorial. Ich frage in einem späteren Spiel noch einmal.", "Vorerst kein Tutorial. Ich frage in einem späteren Spiel noch einmal."},
	.offerNever = {
		"Ich frage nicht mehr. Um das Tutorial zurückzuholen, schalte es in der Einstellungsliste ein, mit {view} und {rb}.",
		"Ich frage nicht mehr. Um das Tutorial zurückzuholen, schalte es in der Einstellungsliste ein, mit Strg plus S."},
};
}
