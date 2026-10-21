#include "tutorial_text.h"

#include <array>
#include <utility>

namespace D2RAccess::TutorialText {
namespace {
// The game's controller glyphs (docs/controller-glyphs.md): U+E000 onward in its font's order.
constexpr std::array<std::pair<std::string_view, uint32_t>, 12> Tokens{{
	{"{dpad}", 0xE000}, {"{up}", 0xE001}, {"{menu}", 0xE005}, {"{view}", 0xE006}, {"{ls}", 0xE007}, {"{rs}", 0xE009},
	{"{lb}", 0xE00B}, {"{rb}", 0xE00C}, {"{a}", 0xE00F}, {"{b}", 0xE010}, {"{x}", 0xE011}, {"{y}", 0xE012}}};

void ReplaceAll(std::string& text, std::string_view token, std::string_view with) {
	for (size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + with.size()))
		text.replace(at, token.size(), with);
}

constexpr Texts English{
	.welcome = {
		"D2R Access tutorial. I will show you how to find your way with the mod until the first quest is done. "
		"Each step asks you to do something; when you do it, a step-done sound plays and the next one comes. "
		"{view} and {rs} left or right go through these messages; {view} and {up} go back to the first. "
		"You can turn the tutorial off in the settings list, with {view} and {rb}.",
		"D2R Access tutorial. I will show you how to find your way with the mod until the first quest is done. "
		"Each step asks you to do something; when you do it, a step-done sound plays and the next one comes. "
		"K and L go through these messages; Shift plus K goes back to the first. "
		"You can turn the tutorial off in the settings list, with Control plus S."},
	.steps = {
		{"First step: your state. Press {view} and {x} to hear your life and mana.",
			"First step: your state. Press Z to hear your life, and X for your mana."},
		{"The radar sounds in four directions as your surroundings change; down sounds lower. I will now play its "
			"sounds, each after its name and what it means; you do not need to press anything.",
			"The radar sounds in four directions as your surroundings change; down sounds lower. I will now play its "
			"sounds, each after its name and what it means; you do not need to press anything."},
		{"Besides the radar, things nearby sound from where they are, louder the closer they are. Until now I kept "
			"them quiet so you could hear the radar alone. I will play each one after its name.",
			"Besides the radar, things nearby sound from where they are, louder the closer they are. Until now I kept "
			"them quiet so you could hear the radar alone. I will play each one after its name."},
		{"The scanner. It lists what is on the map by category: exits, NPCs, waypoints and more, nearest first, with "
			"how far and which way. With {rs}, up and down change the category, and left and right go through it. "
			"Find Warriv in the NPC category: he is the one nearest you.",
			"The scanner. It lists what is on the map by category: exits, NPCs, waypoints and more, nearest first, with "
			"how far and which way. Control plus Page Up and Control plus Page Down change the category; Page Up and "
			"Page Down go through it. Find Warriv in the NPC category: he is the one nearest you."},
		{"Talk to Warriv. He is a few steps away: walk up to him with {ls} the way the scanner said, press {a} beside "
			"him and listen to what he has to tell you. When he is done, close the dialogue with {b}.",
			"Talk to Warriv. He is a few steps away: walk up to him with the arrow keys the way the scanner said, press "
			"D beside him and listen to what he has to tell you. When he is done, close the dialogue with Escape."},
		{"Now Akara, the camp's healer: she has the first quest. She is farther away; find her in the scanner's NPC "
			"category.",
			"Now Akara, the camp's healer: she has the first quest. She is farther away; find her in the scanner's NPC "
			"category."},
		{"The route. It tells you the way to what the scanner named, in directions and distances, as you would plan it "
			"on the minimap; it does not walk for you. With Akara named, press {route}.",
			"The route. It tells you the way to what the scanner named, in directions and distances, as you would plan it "
			"on the minimap; it does not walk for you. With Akara named, press Control plus E."},
		{"The beacon. It pings toward what the scanner named and leads you along the way, but you do the walking. "
			"Press {view} and {lb} to follow Akara, and walk toward the pings until you reach her; another press lets go.",
			"The beacon. It pings toward what the scanner named and leads you along the way, but you do the walking. "
			"Press Control plus B to follow Akara, and walk with the arrow keys toward the pings until you reach her; "
			"another press lets go."},
		{"Talk to Akara: beside her, press {a}. She has a quest for you. "
			"When she is done, close the dialogue with {b}.",
			"Talk to Akara: beside her, press D. She has a quest for you. "
			"When she is done, close the dialogue with Escape."},
		{"Now leave the camp on your own, with what you have learned: among the scanner's exits, find {moor}, "
			"ask for the route, follow the beacon and cross the exit.",
			"Now leave the camp on your own, with what you have learned: among the scanner's exits, find {moor}, "
			"ask for the route, follow the beacon and cross the exit."},
		{"You are in {moor}. Three questions place you: {view} and {y} say the ways out of the area you are in; "
			"{view} and {b}, an overview of the terrain; {view} and {rs} up, the area's description. Try the first two.",
			"You are in {moor}. Two questions place you: Control plus Y says the ways out of the area you are in, "
			"and Control plus O, an overview of the terrain. Try both."},
		{"There are monsters here. They growl as they come near, and the scanner lists them in their own category. "
			"Attack with {a}; your other skills are on the buttons you gave them. If your life drops, drink a potion "
			"with {dpad}. Kill a monster and pick up the gold it drops: go up to it and press {a}.",
			"There are monsters here. They growl as they come near, and the scanner lists them in their own category. "
			"Tab picks the next monster; hold A to attack with your left skill, and W with your right one. If your life "
			"drops, the potions are on keys 1 to 4. Kill a monster and pick up its gold: full stop picks the next item "
			"on the ground and comma the previous one, and D picks it up."},
		{"Find {den}. The map is made anew for each game, so its entrance may be near or far. Follow the radar's "
			"sounds, or the wall tones if you have them on: unexplored passages and floor lead to new ground, and the "
			"radar tells you when it logs a new exit, which the scanner then lists among its exits. If you get lost, "
			"the scanner's unexplored room category shows you where to go. Go into the cave.",
			"Find {den}. The map is made anew for each game, so its entrance may be near or far. Follow the radar's "
			"sounds, or the wall tones if you have them on: unexplored passages and floor lead to new ground, and the "
			"radar tells you when it logs a new exit, which the scanner then lists among its exits. If you get lost, "
			"the scanner's unexplored room category shows you where to go. Go into the cave."},
		{"You are in {den}. As you enter each room you will hear whether parts of it are still unexplored. Kill every "
			"monster in the cave; the game says how many are left. The scanner's unexplored room category helps you miss none.",
			"You are in {den}. As you enter each room you will hear whether parts of it are still unexplored. Kill every "
			"monster in the cave; the game says how many are left. The scanner's unexplored room category helps you miss none."},
		{"The cave is clear. Go back to the camp: open the inventory, use the town portal scroll, and step into the "
			"portal with {a}. You can also walk back with the route and the beacon.",
			"The cave is clear. Go back to the camp: open the inventory with I, go to the town portal scroll and press D "
			"to read it; close the inventory and press D beside the portal. You can also walk back with the route and "
			"the beacon."},
		{"Talk to Akara for your reward, a skill point. Then open the skill tree with {menu} and spend it.",
			"Talk to Akara for your reward, a skill point. Then open the skill tree with T and spend it."},
		{"Tutorial complete. You know how to find your way: scanner, route, beacon and radar. In the settings list, "
			"with {view} and {rb}, you choose what the mod says and plays. From here on, the game is yours.",
			"Tutorial complete. You know how to find your way: scanner, route, beacon and radar. In the settings list, "
			"with Control plus S, you choose what the mod says and plays. The keyboard also has shortcuts that walk "
			"for you: H goes to the nearest unexplored room, and Shift plus Home to what the scanner named. "
			"From here on, the game is yours."}},
	.radarTask = {
		"Now walk with {ls} until you hear a wall.",
		"Now walk with the arrow keys until you hear a wall."},
	.cues = {"Passage: an opening that leads to another room or to an exit",
		"Unexplored passage: the passage with two pips at the end; it leads to ground you have not seen yet",
		"Floor: you can walk that way; further on there may be anything, a passage too",
		"Unexplored floor: the floor with the two pips; ground you have not seen yet lies that way", "Wall: it blocks your way",
		"Door: it opens to let you through", "Small obstacle: something small in your way that you can walk around"},
	.worldCues = {"NPC, someone to talk to", "NPC with an exclamation mark: they have something for you",
		"Stash, the chest where you keep your items", "Stairs, entrance or portal", "Area border, the way into another area",
		"Unexplored room"},
	.worldTask = {
		"Now the ones around you will sound. If you do not know what a sound is, the scanner tells you what is there, "
		"and each sound can be turned off in the settings list.",
		"Now the ones around you will sound. If you do not know what a sound is, the scanner tells you what is there, "
		"and each sound can be turned off in the settings list."},
	.levelUp = {
		"You have gone up a level: you have attribute points and a skill point to spend. Press {menu} to open your "
		"character sheet; I will guide you inside.",
		"You have gone up a level: you have attribute points and a skill point to spend. Press C to open your "
		"character sheet; I will guide you inside."},
	.levelUpSheet = {
		"Your character sheet. You are on Strength's plus: {dpad} up and down go through the four attributes, and "
		"holding {a} spends a point on the one you are on. {rs} up and down read your statistics line by line, and "
		"right says the points you have left.",
		"Your character sheet. The arrow keys take the cursor to each attribute's plus, and A spends a point on it. "
		"J reads the description of the line you are on, and Shift with the up or down arrow goes through it line by "
		"line."},
	.levelUpSwitch = {
		"Your attribute points are spent. Now your skill point: press {rb} until you reach the skill tree; each menu "
		"says its name.",
		"Your attribute points are spent. Now your skill point: press T to open the skill tree beside your sheet."},
	.levelUpTree = {
		"The skill tree. LT and RT change the branch, {dpad} goes through its skills, and {rs} up reads the one you "
		"are on. {a} spends the point on it, and {rs} right says the points you have left. When you are done, close "
		"it with {b}.",
		"The skill tree. Page Up and Page Down change the branch, the arrow keys go through its skills, and J reads "
		"the one the cursor is on. A spends the point on it, and Control plus Page Down says the points you have "
		"left. When you are done, close it with Escape."},
	.levelUpTreeLater = {
		"You still have your skill point. Press {menu}: with the attributes spent, it opens the skill tree.",
		"You still have your skill point. Press T to open the skill tree."},
	.levelUpBind = {
		"To use a skill, put it on a button: in the skill tree, on the skill, press {x} and then the button you "
		"want it on. Each skill's reading says which button it is on.",
		"To use a skill, put it in a hand: S opens the list of skills for the right hand, and Shift plus S the one "
		"for the left; the arrow keys pick one and A puts it in that hand. With a list open, F1 to F8 bind the skill "
		"under the cursor to that key."},
	.kit = {
		"You have picked up something to wear. Press {menu} to open the menus; they open on your inventory, and I "
		"will guide you inside.",
		"You have picked up something to wear. Press I to open your inventory; I will guide you inside."},
	.kitOpen = {
		"Your inventory. {dpad} goes from cell to cell, through your backpack and what you have on, and the mod says "
		"what each holds. On an item, {rs} up reads its description, and {rs} down the buttons you can use with it, "
		"the one that equips it among them. An item that says unidentified cannot be worn until it is identified, "
		"with a scroll of identify, which Akara sells.",
		"Your inventory. The arrow keys take the cursor from cell to cell, through your backpack and what you have "
		"on, and the mod says what each holds. J reads an item's description, and Shift plus A equips it. An item "
		"that says unidentified cannot be worn until it is identified, with a scroll of identify, which Akara sells."},
	.kitWorn = {
		"You have it on. Close the inventory with {b} when you are done.",
		"You have it on. Close the inventory with Escape when you are done."},
	.kitDone = {
		"Done. Whatever you pick up, the inventory is where you put it on. Back to what you were doing:",
		"Done. Whatever you pick up, the inventory is where you put it on. Back to what you were doing:"},
	.levelUpDone = {
		"Done. Whenever you go up a level, {menu} opens your character sheet. Back to what you were doing:",
		"Done. Whenever you go up a level, C opens your character sheet and T the skill tree. Back to what you were "
		"doing:"},
	.routeStick = "{view} and {rs} down",
	.routeButton = "{view} and {a}",
	.firstMessage = "First message",
	.lastMessage = "Last message",
	.den = "Den of Evil",
	.moor = "Blood Moor",
};

constexpr Texts Spanish{
	.welcome = {
		"Tutorial de D2R Access. Te enseñaré a orientarte con el mod hasta completar la primera misión. "
		"Cada paso te pide algo; al hacerlo sonará un aviso de paso completado y llegará el siguiente. "
		"{view} y {rs} a la izquierda o a la derecha recorren estos mensajes; {view} y {up} vuelven al primero. "
		"Puedes apagar el tutorial en la lista de ajustes, con {view} y {rb}.",
		"Tutorial de D2R Access. Te enseñaré a orientarte con el mod hasta completar la primera misión. "
		"Cada paso te pide algo; al hacerlo sonará un aviso de paso completado y llegará el siguiente. "
		"K y L recorren estos mensajes; Mayúsculas más K vuelve al primero. "
		"Puedes apagar el tutorial en la lista de ajustes, con Control más S."},
	.steps = {
		{"Primer paso: tu estado. Pulsa {view} y {x} para oír tu vida y tu maná.",
			"Primer paso: tu estado. Pulsa Z para oír tu vida, y X para tu maná."},
		{"El radar suena en cuatro direcciones cuando cambia lo que te rodea; abajo, más grave. Ahora te reproduzco "
			"sus sonidos, cada uno después de su nombre y de lo que significa; no tienes que pulsar nada.",
			"El radar suena en cuatro direcciones cuando cambia lo que te rodea; abajo, más grave. Ahora te reproduzco "
			"sus sonidos, cada uno después de su nombre y de lo que significa; no tienes que pulsar nada."},
		{"Además del radar, lo que hay cerca suena desde donde está, más fuerte cuanto más cerca. Hasta ahora lo "
			"tenía callado para que oyeras el radar solo. Te reproduzco cada sonido después de su nombre.",
			"Además del radar, lo que hay cerca suena desde donde está, más fuerte cuanto más cerca. Hasta ahora lo "
			"tenía callado para que oyeras el radar solo. Te reproduzco cada sonido después de su nombre."},
		{"El escáner. Lista lo que hay en el mapa por categorías: salidas, PNJ, puntos de viaje y más, del más "
			"cercano al más lejano, con su distancia y dirección. Con {rs}, arriba y abajo cambian de categoría, e "
			"izquierda y derecha la recorren. Busca a Warriv en la categoría PNJ: es el que tienes más cerca.",
			"El escáner. Lista lo que hay en el mapa por categorías: salidas, PNJ, puntos de viaje y más, del más "
			"cercano al más lejano, con su distancia y dirección. Control más Re Pág y Control más Av Pág cambian de "
			"categoría; Re Pág y Av Pág la recorren. Busca a Warriv en la categoría PNJ: es el que tienes más cerca."},
		{"Habla con Warriv. Está a pocos pasos: acércate con {ls} hacia donde dijo el escáner, pulsa {a} a su lado y "
			"escucha lo que tiene que decirte. Cuando termine, cierra el diálogo con {b}.",
			"Habla con Warriv. Está a pocos pasos: acércate con las flechas hacia donde dijo el escáner, pulsa D a su "
			"lado y escucha lo que tiene que decirte. Cuando termine, cierra el diálogo con Escape."},
		{"Ahora Akara, la sanadora del campamento: tiene la primera misión. Está más lejos; búscala en la categoría "
			"PNJ del escáner.",
			"Ahora Akara, la sanadora del campamento: tiene la primera misión. Está más lejos; búscala en la categoría "
			"PNJ del escáner."},
		{"La ruta. Te dice el camino hasta lo que nombró el escáner, en direcciones y distancias, como se planea con "
			"el minimapa; no camina por ti. Con Akara nombrada, pulsa {route}.",
			"La ruta. Te dice el camino hasta lo que nombró el escáner, en direcciones y distancias, como se planea con "
			"el minimapa; no camina por ti. Con Akara nombrada, pulsa Control más E."},
		{"La baliza. Pita hacia lo que nombró el escáner y te guía por el camino, pero andas tú. Pulsa {view} y {lb} "
			"para seguir a Akara y camina hacia los pitidos hasta llegar a ella; otra pulsación la suelta.",
			"La baliza. Pita hacia lo que nombró el escáner y te guía por el camino, pero andas tú. Pulsa Control más B "
			"para seguir a Akara y camina con las flechas hacia los pitidos hasta llegar a ella; otra pulsación la suelta."},
		{"Habla con Akara: a su lado, pulsa {a}. Tiene una misión para ti. "
			"Cuando termine, cierra el diálogo con {b}.",
			"Habla con Akara: a su lado, pulsa D. Tiene una misión para ti. "
			"Cuando termine, cierra el diálogo con Escape."},
		{"Ahora sal del campamento tú solo, con lo aprendido: entre las salidas del escáner, busca {moor}, pide la "
			"ruta, sigue la baliza y cruza la salida.",
			"Ahora sal del campamento tú solo, con lo aprendido: entre las salidas del escáner, busca {moor}, pide la "
			"ruta, sigue la baliza y cruza la salida."},
		{"Estás en la zona {moor}. Tres consultas te sitúan: {view} y {y} dicen las salidas de la zona donde "
			"estás; {view} y {b}, un resumen del terreno; {view} y {rs} hacia arriba, la descripción del área. "
			"Prueba las dos primeras.",
			"Estás en la zona {moor}. Dos consultas te sitúan: Control más Y dice las salidas de la zona donde "
			"estás, y Control más O, un resumen del terreno. Prueba las dos."},
		{"Aquí hay monstruos. Gruñen al acercarse, y el escáner los lista en su propia categoría. Ataca con {a}; tus "
			"otras habilidades van en los botones que les diste. Si la vida baja, bebe una poción con {dpad}. Mata algún "
			"monstruo y recoge el oro que suelte: acércate y pulsa {a}.",
			"Aquí hay monstruos. Gruñen al acercarse, y el escáner los lista en su propia categoría. Tabulador elige el "
			"monstruo siguiente; mantén A para atacar con tu habilidad izquierda, y W con la derecha. Si la vida baja, "
			"las pociones van en las teclas del 1 al 4. Mata algún monstruo y recoge su oro: Punto elige el objeto del "
			"suelo siguiente y Coma el anterior, y D lo recoge."},
		{"Busca {den}. El mapa se genera de nuevo en cada partida, así que su entrada puede estar cerca o lejos. "
			"Guíate por los sonidos del radar, o por los tonos de las paredes si los tienes activados: los pasajes y el "
			"suelo sin explorar llevan a terreno nuevo, y el radar avisa cuando registra una salida nueva, que el "
			"escáner lista después entre sus salidas. Si te pierdes, la categoría sala inexplorada del escáner te dice "
			"adónde ir. Entra en la cueva.",
			"Busca {den}. El mapa se genera de nuevo en cada partida, así que su entrada puede estar cerca o lejos. "
			"Guíate por los sonidos del radar, o por los tonos de las paredes si los tienes activados: los pasajes y el "
			"suelo sin explorar llevan a terreno nuevo, y el radar avisa cuando registra una salida nueva, que el "
			"escáner lista después entre sus salidas. Si te pierdes, la categoría sala inexplorada del escáner te dice "
			"adónde ir. Entra en la cueva."},
		{"Estás en {den}. Al entrar en cada sala oirás si le quedan partes sin explorar. Mata a todos los "
			"monstruos de la cueva; el juego dice cuántos quedan. La categoría sala inexplorada del escáner te ayuda a "
			"no dejarte ninguno.",
			"Estás en {den}. Al entrar en cada sala oirás si le quedan partes sin explorar. Mata a todos los "
			"monstruos de la cueva; el juego dice cuántos quedan. La categoría sala inexplorada del escáner te ayuda a "
			"no dejarte ninguno."},
		{"La cueva está limpia. Vuelve al campamento: abre el inventario, usa el pergamino de portal y entra en el "
			"portal con {a}. También puedes volver andando con la ruta y la baliza.",
			"La cueva está limpia. Vuelve al campamento: abre el inventario con I, ve al pergamino de portal y pulsa D "
			"para leerlo; cierra el inventario y pulsa D junto al portal. También puedes volver andando con la ruta y "
			"la baliza."},
		{"Habla con Akara para recibir tu recompensa, un punto de habilidad. Después abre el árbol de habilidades con "
			"{menu} y gástalo.",
			"Habla con Akara para recibir tu recompensa, un punto de habilidad. Después abre el árbol de habilidades con "
			"T y gástalo."},
		{"Tutorial completado. Ya sabes orientarte: escáner, ruta, baliza y radar. En la lista de ajustes, con {view} "
			"y {rb}, eliges qué dice y qué suena el mod. Desde aquí, el juego es tuyo.",
			"Tutorial completado. Ya sabes orientarte: escáner, ruta, baliza y radar. En la lista de ajustes, con "
			"Control más S, eliges qué dice y qué suena el mod. El teclado tiene además atajos que caminan por ti: H va "
			"a la sala sin explorar más cercana, y Mayúsculas más Inicio a lo que nombró el escáner. "
			"Desde aquí, el juego es tuyo."}},
	.radarTask = {
		"Ahora camina con {ls} hasta oír una pared.",
		"Ahora camina con las flechas hasta oír una pared."},
	.cues = {"Pasaje: un hueco que lleva a otra sala o a una salida",
		"Pasaje sin explorar: el pasaje con dos pitidos al final; lleva a terreno que aún no has visto",
		"Suelo: por ahí puedes andar; más adelante puede haber de todo, también un pasaje",
		"Suelo sin explorar: el suelo con los dos pitidos; por ahí hay terreno que aún no has visto", "Pared: te corta el paso",
		"Puerta: se abre para pasar", "Obstáculo pequeño: algo pequeño que te corta el paso y que puedes rodear"},
	.worldCues = {"PNJ, alguien con quien hablar", "PNJ con una exclamación: tiene algo para ti",
		"Alijo, el cofre donde guardas tus objetos", "Escaleras, entrada o portal", "Borde de zona, el paso a otra zona",
		"Sala sin explorar"},
	.worldTask = {
		"Ahora sonarán los que tienes alrededor. Si no sabes qué es un sonido, el escáner te dice lo que hay, y cada "
		"sonido se puede apagar en la lista de ajustes.",
		"Ahora sonarán los que tienes alrededor. Si no sabes qué es un sonido, el escáner te dice lo que hay, y cada "
		"sonido se puede apagar en la lista de ajustes."},
	.levelUp = {
		"Has subido de nivel: tienes puntos de atributo y un punto de habilidad para gastar. Pulsa {menu} para abrir "
		"tu ficha; te guío dentro.",
		"Has subido de nivel: tienes puntos de atributo y un punto de habilidad para gastar. Pulsa C para abrir tu "
		"ficha; te guío dentro."},
	.levelUpSheet = {
		"Tu ficha. Estás en el más de Fuerza: con {dpad} arriba y abajo pasas por los cuatro atributos, y manteniendo "
		"{a} gastas un punto en el que estés. Con {rs} arriba y abajo lees tus estadísticas línea a línea, y a la "
		"derecha, los puntos que te quedan.",
		"Tu ficha. Las flechas llevan el cursor al más de cada atributo, y A gasta un punto en él. J lee la "
		"descripción de la línea en la que estás, y Mayúsculas con flecha arriba o abajo la recorre línea a línea."},
	.levelUpSwitch = {
		"Ya has repartido los atributos. Ahora tu punto de habilidad: pulsa {rb} hasta llegar al árbol de "
		"habilidades; cada menú dice su nombre.",
		"Ya has repartido los atributos. Ahora tu punto de habilidad: pulsa T para abrir el árbol de habilidades junto "
		"a tu ficha."},
	.levelUpTree = {
		"El árbol de habilidades. LT y RT cambian de rama, {dpad} recorre sus habilidades y {rs} arriba lee la que "
		"tienes. {a} gasta el punto en ella, y {rs} a la derecha dice los puntos que te quedan. Cuando acabes, "
		"ciérralo con {b}.",
		"El árbol de habilidades. Re Pág y Av Pág cambian de rama, las flechas recorren sus habilidades y J lee la "
		"que tiene el cursor. A gasta el punto en ella, y Control más Av Pág dice los puntos que te quedan. Cuando "
		"acabes, ciérralo con Escape."},
	.levelUpTreeLater = {
		"Aún te queda el punto de habilidad. Pulsa {menu}: con los atributos repartidos, abre el árbol de habilidades.",
		"Aún te queda el punto de habilidad. Pulsa T para abrir el árbol de habilidades."},
	.levelUpBind = {
		"Para usar una habilidad, ponla en un botón: en el árbol de habilidades, sobre ella, pulsa {x} y después el "
		"botón en el que la quieras. La lectura de cada habilidad dice en qué botón está.",
		"Para usar una habilidad, ponla en una mano: S abre la lista de habilidades de la mano derecha, y Mayúsculas "
		"más S la de la izquierda; las flechas eligen una y A la pone en esa mano. Con una lista abierta, F1 a F8 "
		"asignan a esa tecla la habilidad que tiene el cursor."},
	.kit = {
		"Has recogido algo que te puedes poner. Pulsa {menu} para abrir los menús; se abren en tu inventario, y te "
		"guío dentro.",
		"Has recogido algo que te puedes poner. Pulsa I para abrir tu inventario; te guío dentro."},
	.kitOpen = {
		"Tu inventario. Con {dpad} vas de casilla en casilla, por tu mochila y por lo que llevas puesto, y el mod dice "
		"qué hay en cada una. En un objeto, {rs} arriba lee su descripción, y {rs} abajo, los botones que puedes usar "
		"con él, entre ellos el de equiparlo. Un objeto que dice sin identificar no se puede equipar hasta "
		"identificarlo, con un pergamino de identificación, que vende Akara.",
		"Tu inventario. Las flechas llevan el cursor de casilla en casilla, por tu mochila y por lo que llevas puesto, "
		"y el mod dice qué hay en cada una. J lee la descripción de un objeto, y Mayúsculas más A lo equipa. Un objeto "
		"que dice sin identificar no se puede equipar hasta identificarlo, con un pergamino de identificación, que "
		"vende Akara."},
	.kitWorn = {
		"Ya lo llevas puesto. Cierra el inventario con {b} cuando acabes.",
		"Ya lo llevas puesto. Cierra el inventario con Escape cuando acabes."},
	.kitDone = {
		"Hecho. Lo que recojas, en el inventario te lo pones. Volvemos a lo que estabas haciendo:",
		"Hecho. Lo que recojas, en el inventario te lo pones. Volvemos a lo que estabas haciendo:"},
	.levelUpDone = {
		"Hecho. Cada vez que subas de nivel, {menu} abre tu ficha. Volvemos a lo que estabas haciendo:",
		"Hecho. Cada vez que subas de nivel, C abre tu ficha y T el árbol de habilidades. Volvemos a lo que estabas "
		"haciendo:"},
	.routeStick = "{view} y {rs} hacia abajo",
	.routeButton = "{view} y {a}",
	.firstMessage = "Primer mensaje",
	.lastMessage = "Último mensaje",
	// Spain's names (game-data/local/lng/strings/levels.json); Mexico's are Guarida del Mal and Páramo Sangriento.
	.den = "Cubil del Mal",
	.moor = "Landa de Sangre",
};
}

const Texts& For(Strings::Language language) {
	using L = Strings::Language;
	return language == L::esES || language == L::esMX ? Spanish : English;
}

std::string Render(std::string_view text, const Texts& texts, bool routeOnA, const Places& places,
	const std::function<std::string(uint32_t)>& button) {
	std::string result(text);
	ReplaceAll(result, "{den}", places.den.empty() ? std::string_view(texts.den) : std::string_view(places.den));
	ReplaceAll(result, "{moor}", places.moor.empty() ? std::string_view(texts.moor) : std::string_view(places.moor));
	ReplaceAll(result, "{route}", routeOnA ? texts.routeButton : texts.routeStick);
	for (const auto& [token, glyph] : Tokens)
		if (result.find(token) != std::string::npos) ReplaceAll(result, token, button(glyph));
	return result;
}
}
