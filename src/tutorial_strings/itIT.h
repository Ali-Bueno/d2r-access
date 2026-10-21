#pragma once
#include "tutorial_text.h"

// Italian (itIT): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts itIT{
	.welcome = {
		"Tutorial di D2R Access. Ti mostrerò come orientarti con la mod fino al completamento della prima missione. "
		"Ogni passo ti chiede di fare qualcosa; quando lo fai, si sente il suono di passo completato e arriva il successivo. "
		"{view} e {rs} a sinistra o a destra scorrono questi messaggi; {view} e {up} tornano al primo. "
		"Puoi disattivare il tutorial nell'elenco delle impostazioni, con {view} e {rb}.",
		"Tutorial di D2R Access. Ti mostrerò come orientarti con la mod fino al completamento della prima missione. "
		"Ogni passo ti chiede di fare qualcosa; quando lo fai, si sente il suono di passo completato e arriva il successivo. "
		"K e L scorrono questi messaggi; Maiusc più K torna al primo. "
		"Puoi disattivare il tutorial nell'elenco delle impostazioni, con Ctrl più S."},
	.steps = {
		{"Primo passo: il tuo stato. Premi {view} e {x} per sentire la tua vita e il tuo mana.",
			"Primo passo: il tuo stato. Premi Z per sentire la tua vita, e X per il tuo mana."},
		{"Il radar suona in quattro direzioni mentre cambia ciò che ti circonda; verso il basso suona più grave. Ora riprodurrò "
			"i suoi suoni, ciascuno dopo il suo nome e il suo significato; non devi premere nulla.",
			"Il radar suona in quattro direzioni mentre cambia ciò che ti circonda; verso il basso suona più grave. Ora riprodurrò "
			"i suoi suoni, ciascuno dopo il suo nome e il suo significato; non devi premere nulla."},
		{"Oltre al radar, le cose vicine suonano dal punto in cui si trovano, più forte quanto più sono vicine. Finora le ho tenute "
			"basse perché potessi sentire il radar da solo. Riprodurrò ciascuna dopo il suo nome.",
			"Oltre al radar, le cose vicine suonano dal punto in cui si trovano, più forte quanto più sono vicine. Finora le ho tenute "
			"basse perché potessi sentire il radar da solo. Riprodurrò ciascuna dopo il suo nome."},
		{"Lo scanner. Elenca ciò che c'è sulla mappa per categoria: uscite, PNG, waypoint e altro, dal più vicino, con "
			"distanza e direzione. Con {rs}, su e giù cambiano la categoria, e sinistra e destra la scorrono. "
			"Trova Warriv nella categoria dei PNG: è il più vicino a te.",
			"Lo scanner. Elenca ciò che c'è sulla mappa per categoria: uscite, PNG, waypoint e altro, dal più vicino, con "
			"distanza e direzione. Ctrl più Pagina su e Ctrl più Pagina giù cambiano la categoria; Pagina su e "
			"Pagina giù la scorrono. Trova Warriv nella categoria dei PNG: è il più vicino a te."},
		{"Parla con Warriv. È a pochi passi: avvicinati con {ls} nel modo indicato dallo scanner, premi {a} accanto a "
			"lui e ascolta ciò che ha da dirti. Quando ha finito, chiudi il dialogo con {b}.",
			"Parla con Warriv. È a pochi passi: avvicinati con i tasti freccia nel modo indicato dallo scanner, premi "
			"D accanto a lui e ascolta ciò che ha da dirti. Quando ha finito, chiudi il dialogo con Esc."},
		{"Ora Akara, la guaritrice del campo: ha la prima missione. È più lontana; trovala nella categoria dei PNG "
			"dello scanner.",
			"Ora Akara, la guaritrice del campo: ha la prima missione. È più lontana; trovala nella categoria dei PNG "
			"dello scanner."},
		{"Il percorso. Ti dice la strada verso ciò che lo scanner ha nominato, con direzioni e distanze, come la pianificheresti "
			"sulla minimappa; non cammina al posto tuo. Con Akara nominata, premi {route}.",
			"Il percorso. Ti dice la strada verso ciò che lo scanner ha nominato, con direzioni e distanze, come la pianificheresti "
			"sulla minimappa; non cammina al posto tuo. Con Akara nominata, premi Ctrl più E."},
		{"La guida. Emette segnali verso ciò che lo scanner ha nominato e ti accompagna lungo la strada, "
			"ma sei tu a camminare. Premi {view} e {lb} per seguire Akara, e cammina verso i segnali finché non la raggiungi; "
			"un'altra pressione la lascia andare.",
			"La guida. Emette segnali verso ciò che lo scanner ha nominato e ti accompagna lungo la strada, "
			"ma sei tu a camminare. Premi Ctrl più B per seguire Akara, e cammina con i tasti freccia verso i segnali finché non la raggiungi; "
			"un'altra pressione la lascia andare."},
		{"Parla con Akara: accanto a lei, premi {a}. Ha una missione per te. "
			"Quando ha finito, chiudi il dialogo con {b}.",
			"Parla con Akara: accanto a lei, premi D. Ha una missione per te. "
			"Quando ha finito, chiudi il dialogo con Esc."},
		{"Ora lascia il campo da solo, con ciò che hai imparato: tra le uscite dello scanner, trova {moor}, "
			"chiedi il percorso, segui la guida e attraversa l'uscita.",
			"Ora lascia il campo da solo, con ciò che hai imparato: tra le uscite dello scanner, trova {moor}, "
			"chiedi il percorso, segui la guida e attraversa l'uscita."},
		{"Sei in {moor}. Tre domande ti collocano: {view} e {y} dicono le vie d'uscita dell'area in cui ti trovi; "
			"{view} e {b}, una panoramica del terreno; {view} e {rs} in su, la descrizione dell'area. Prova le prime due.",
			"Sei in {moor}. Due domande ti collocano: Ctrl più Y dice le vie d'uscita dell'area in cui ti trovi, "
			"e Ctrl più O, una panoramica del terreno. Provale entrambe."},
		{"Qui ci sono dei mostri. Ringhiano quando si avvicinano, e lo scanner li elenca in una categoria a parte. "
			"Attacca con {a}; le tue altre abilità sono sui pulsanti che hai assegnato loro. Se la tua vita cala, bevi una pozione "
			"con {dpad}. Uccidi un mostro e raccogli l'oro che lascia: avvicinati e premi {a}.",
			"Qui ci sono dei mostri. Ringhiano quando si avvicinano, e lo scanner li elenca in una categoria a parte. "
			"Tab sceglie il mostro successivo; tieni premuto A per attaccare con l'abilità sinistra, e W con la destra. Se la tua vita "
			"cala, le pozioni sono sui tasti da 1 a 4. Uccidi un mostro e raccogli il suo oro: punto sceglie l'oggetto successivo "
			"a terra e virgola il precedente, e D lo raccoglie."},
		{"Trova {den}. La mappa viene rifatta a ogni partita, quindi il suo ingresso può essere vicino o lontano. Segui i suoni "
			"del radar, o i toni dei muri se li hai attivi: i passaggi inesplorati e il pavimento inesplorato portano a terreno nuovo, e il "
			"radar ti avvisa quando registra una nuova uscita, che lo scanner poi elenca tra le sue uscite. Se ti perdi, "
			"la categoria stanza inesplorata dello scanner ti mostra dove andare. Entra nella caverna.",
			"Trova {den}. La mappa viene rifatta a ogni partita, quindi il suo ingresso può essere vicino o lontano. Segui i suoni "
			"del radar, o i toni dei muri se li hai attivi: i passaggi inesplorati e il pavimento inesplorato portano a terreno nuovo, e il "
			"radar ti avvisa quando registra una nuova uscita, che lo scanner poi elenca tra le sue uscite. Se ti perdi, "
			"la categoria stanza inesplorata dello scanner ti mostra dove andare. Entra nella caverna."},
		{"Sei in {den}. Entrando in ogni stanza sentirai se parti di essa sono ancora inesplorate. Uccidi tutti i "
			"mostri nella caverna; il gioco dice quanti ne restano. La categoria stanza inesplorata dello scanner ti aiuta a non perderne nessuno.",
			"Sei in {den}. Entrando in ogni stanza sentirai se parti di essa sono ancora inesplorate. Uccidi tutti i "
			"mostri nella caverna; il gioco dice quanti ne restano. La categoria stanza inesplorata dello scanner ti aiuta a non perderne nessuno."},
		{"La caverna è libera. Torna al campo: apri l'inventario, usa la pergamena del Portale Cittadino ed entra nel "
			"portale con {a}. Puoi anche tornare a piedi con il percorso e la guida.",
			"La caverna è libera. Torna al campo: apri l'inventario con I, vai alla pergamena del Portale Cittadino e premi D "
			"per leggerla; chiudi l'inventario e premi D accanto al portale. Puoi anche tornare a piedi con il percorso e "
			"la guida."},
		{"Parla con Akara per la tua ricompensa, un punto abilità. Poi apri l'albero delle abilità con {menu} e spendilo.",
			"Parla con Akara per la tua ricompensa, un punto abilità. Poi apri l'albero delle abilità con T e spendilo."},
		{"Tutorial completato. Sai come orientarti: scanner, percorso, guida e radar. Nell'elenco delle impostazioni, "
			"con {view} e {rb}, scegli cosa dice e riproduce la mod. Da qui in poi il gioco è tuo.",
			"Tutorial completato. Sai come orientarti: scanner, percorso, guida e radar. Nell'elenco delle impostazioni, "
			"con Ctrl più S, scegli cosa dice e riproduce la mod. La tastiera ha anche scorciatoie che camminano "
			"al posto tuo: H va alla stanza inesplorata più vicina, e Maiusc più Home a ciò che lo scanner ha nominato. "
			"Da qui in poi il gioco è tuo."}},
	.radarTask = {
		"Ora cammina con {ls} finché non senti un muro.",
		"Ora cammina con i tasti freccia finché non senti un muro."},
	.cues = {"Passaggio: un'apertura che porta a un'altra stanza o a un'uscita",
		"Passaggio inesplorato: il passaggio con due beep alla fine; porta a terreno che non hai ancora visto",
		"Pavimento libero: puoi camminare in quella direzione; più avanti può esserci di tutto, anche un passaggio",
		"Pavimento inesplorato: il pavimento con i due beep; in quella direzione c'è terreno che non hai ancora visto", "Muro: ti blocca la strada",
		"Porta: si apre per lasciarti passare", "Piccolo ostacolo: qualcosa di piccolo sulla tua strada che puoi aggirare"},
	.worldCues = {"PNG, qualcuno con cui parlare", "PNG con punto esclamativo: ha qualcosa per te",
		"Scrigno personale, il forziere dove tieni i tuoi oggetti", "Scale, ingresso o portale", "Bordo dell'area, la via verso un'altra area",
		"Stanza inesplorata"},
	.worldTask = {
		"Ora suoneranno quelli intorno a te. Se non sai cos'è un suono, lo scanner ti dice cosa c'è lì, "
		"e ogni suono può essere disattivato nell'elenco delle impostazioni.",
		"Ora suoneranno quelli intorno a te. Se non sai cos'è un suono, lo scanner ti dice cosa c'è lì, "
		"e ogni suono può essere disattivato nell'elenco delle impostazioni."},
	.levelUp = {
		"Hai raggiunto un nuovo livello: hai punti caratteristica e un punto abilità da spendere. Premi {menu} per aprire la "
		"schermata del personaggio; ti guiderò al suo interno.",
		"Hai raggiunto un nuovo livello: hai punti caratteristica e un punto abilità da spendere. Premi C per aprire la "
		"schermata del personaggio; ti guiderò al suo interno."},
	.levelUpSheet = {
		"La schermata del personaggio. Sei sul più della Forza: {dpad} su e giù scorrono le quattro caratteristiche, e "
		"tenere premuto {a} spende un punto su quella in cui ti trovi. {rs} su e giù leggono le tue statistiche riga per riga, e "
		"a destra dice i punti che ti restano.",
		"La schermata del personaggio. I tasti freccia portano il cursore sul più di ogni caratteristica, e A vi spende un punto. "
		"J legge la descrizione della riga in cui ti trovi, e Maiusc con la freccia su o giù la scorre riga per "
		"riga."},
	.levelUpSwitch = {
		"I tuoi punti caratteristica sono spesi. Ora il tuo punto abilità: premi {rb} finché non arrivi all'albero delle abilità; ogni menu "
		"dice il suo nome.",
		"I tuoi punti caratteristica sono spesi. Ora il tuo punto abilità: premi T per aprire l'albero delle abilità accanto alla schermata del personaggio."},
	.levelUpTree = {
		"L'albero delle abilità. LT e RT cambiano ramo, {dpad} scorre le sue abilità, e {rs} in su legge quella in cui ti "
		"trovi. {a} vi spende il punto, e {rs} a destra dice i punti che ti restano. Quando hai finito, chiudilo "
		"con {b}.",
		"L'albero delle abilità. Pagina su e Pagina giù cambiano ramo, i tasti freccia scorrono le sue abilità, e J legge "
		"quella su cui si trova il cursore. A vi spende il punto, e Ctrl più Pagina giù dice i punti che ti "
		"restano. Quando hai finito, chiudilo con Esc."},
	.levelUpTreeLater = {
		"Hai ancora il tuo punto abilità. Premi {menu}: con le caratteristiche spese, apre l'albero delle abilità.",
		"Hai ancora il tuo punto abilità. Premi T per aprire l'albero delle abilità."},
	.levelUpBind = {
		"Per usare un'abilità, mettila su un pulsante: nell'albero delle abilità, sull'abilità, premi {x} e poi il pulsante "
		"che vuoi. La lettura di ogni abilità dice su quale pulsante si trova.",
		"Per usare un'abilità, mettila in una mano: S apre l'elenco delle abilità per la mano destra, e Maiusc più S quello "
		"per la sinistra; i tasti freccia ne scelgono una e A la mette in quella mano. Con un elenco aperto, da F1 a F8 associano l'abilità "
		"sotto il cursore a quel tasto."},
	.kit = {
		"Hai raccolto qualcosa da indossare. Premi {menu} per aprire i menu; si aprono sul tuo inventario, e ti "
		"guiderò al suo interno.",
		"Hai raccolto qualcosa da indossare. Premi I per aprire il tuo inventario; ti guiderò al suo interno."},
	.kitOpen = {
		"Il tuo inventario. {dpad} va da casella a casella, nello zaino e su ciò che indossi, e la mod dice "
		"cosa contiene ognuna. Su un oggetto, {rs} in su legge la sua descrizione, e {rs} in giù i pulsanti che puoi usare con esso, "
		"tra cui quello che lo equipaggia. Un oggetto che dice non identificato non può essere indossato finché non viene identificato, "
		"con una pergamena dell'Identificazione, che Akara vende.",
		"Il tuo inventario. I tasti freccia portano il cursore da casella a casella, nello zaino e su ciò che "
		"indossi, e la mod dice cosa contiene ognuna. J legge la descrizione di un oggetto, e Maiusc più A lo equipaggia. Un oggetto "
		"che dice non identificato non può essere indossato finché non viene identificato, con una pergamena dell'Identificazione, che Akara vende."},
	.kitWorn = {
		"Lo indossi. Chiudi l'inventario con {b} quando hai finito.",
		"Lo indossi. Chiudi l'inventario con Esc quando hai finito."},
	.kitDone = {
		"Fatto. Qualunque cosa raccolga, l'inventario è il posto dove la indossi. Torniamo a ciò che stavi facendo:",
		"Fatto. Qualunque cosa raccolga, l'inventario è il posto dove la indossi. Torniamo a ciò che stavi facendo:"},
	.levelUpDone = {
		"Fatto. Ogni volta che sali di livello, {menu} apre la schermata del personaggio. Torniamo a ciò che stavi facendo:",
		"Fatto. Ogni volta che sali di livello, C apre la schermata del personaggio e T l'albero delle abilità. Torniamo a ciò che stavi "
		"facendo:"},
	.routeStick = "{view} e {rs} in giù",
	.routeButton = "{view} e {a}",
	.firstMessage = "Primo messaggio",
	.lastMessage = "Ultimo messaggio",
	.den = "Antro del Male",
	.moor = "Lande Insanguinate",
};
}
